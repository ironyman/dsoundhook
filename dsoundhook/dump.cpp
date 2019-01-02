#include "stdafx.h"
#include <Windows.h>
#include <winerror.h>
#include <cstdint>
#include <cassert>
#include <shlwapi.h>
#include <strsafe.h>
#include <iostream>

#include "my_exception.h"

#pragma comment(lib, "Shlwapi.lib")

#define DUMP_DIR L"C:\\dump\\"

#pragma pack(push, 1)
typedef struct _WavHeader
{
	uint8_t sGroupId[4]; // "RIFF"
	uint32_t dwFileLength; // File size - 8
	uint8_t sRiffType[4]; // "WAVE"
} WavHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct _WavFormatChunk
{
	uint8_t sGroupID[4]; // Contains "fmt " (includes trailing space)
	uint32_t dwChunkSize; // Should be 16 for PCM
	uint16_t wFormatTag; // Should be 1 for PCM. 3 for IEEE Float
	uint16_t wChannels;
	uint32_t dwSamplesPerSec;
	uint32_t dwAvgBytesPerSec; // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
	uint16_t wBlockAlign; // num_channels * Bytes Per Sample
	uint16_t dwBitsPerSample; // Number of bits per sample
} WavFormatChunk;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct _WavDataChunk
{
	uint8_t sGroupID[4]; // Contains "data"
	uint32_t dwChunkSize; // Number of bytes in data. Number of samples * wChannels * sample byte size
//    uint8_t sampleData[0];
} WavDataChunk;
#pragma pack(pop)

static BOOL CALLBACK GetMyMainWindowCallback(HWND hwnd, LPARAM lParam)
{
	DWORD processId = 0;

	GetWindowThreadProcessId(hwnd, &processId);
	if (processId == GetCurrentProcessId() &&
		GetWindow(hwnd, GW_OWNER) == NULL &&
		IsWindowVisible(hwnd))
	{
		HWND* result = reinterpret_cast<HWND*>(lParam);
		*result = hwnd;
		return false;
	}
	return true;
}

static HWND GetMyMainWindow()
{
	HWND myWindow = NULL;

	// auto GetMyMainWindowCallback = [](HWND hwnd, LPARAM lParam) -> BOOL
	// {
	//     DWORD processId = 0;
	//     GetWindowThreadProcessId(hwnd, &processId);
	//     if (processId == GetCurrentProcessId() &&
	//         GetWindow(hwnd, GW_OWNER) == nullptr &&
	//         IsWindowVisible(hwnd))
	//     {
	//         HWND& result = *reinterpret_cast<HWND*>(lParam);
	//         result = hwnd;
	//         return false;
	//     }
	//     return true;
	// };

	// This guy's return value is stupid.
	EnumWindows(GetMyMainWindowCallback, reinterpret_cast<LPARAM>(&myWindow));
	if (!myWindow)
	{
		// MessageBox(nullptr, L"Could not find my main window.", L"Hook", MB_ICONSTOP);
		return NULL;
	}
	return myWindow;
}

// Free me!
static LPTSTR GetCurrentSong()
{
	HWND hwnd = GetMyMainWindow();

	// Main window is exiting!
	if (!hwnd)
	{
		return nullptr;
	}

	int textLen = GetWindowTextLength(hwnd);
	assert(textLen > 0);
	LPTSTR text = static_cast<LPTSTR>(malloc(textLen * sizeof(TCHAR) + sizeof(TCHAR)));

	if (GetWindowText(hwnd, text, textLen + 1) == 0)
	{
		// MessageBox(nullptr, L"Failed to get window title text.", L"Hook", MB_ICONSTOP);
		return nullptr;
	}

	return text;
}

static void WriteInitialDataChunk(LPTSTR dumpPath)
{
	void *chunkBuf = malloc(sizeof(WavDataChunk));
	WavDataChunk *chunk = reinterpret_cast<WavDataChunk *>(chunkBuf);
	memcpy(chunk->sGroupID, "data", 4);
	chunk->dwChunkSize = 0;

	HANDLE hFile{ INVALID_HANDLE_VALUE };
	DWORD written{ 0 };
	INT failCount{ 0 };
	do
	{
		hFile = CreateFile(dumpPath, FILE_APPEND_DATA, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() != ERROR_SHARING_VIOLATION)
			{
				std::cerr << "Failed to open file in " << __FUNCTION__ << ": 0x" << GetLastError() << "\n";
				MessageBox(nullptr, L"Failed open file.", L"Hook", MB_ICONSTOP);
				exit(-12);
			}
			if (++failCount == 10)
			{
				std::cerr << "Failed to open file in " << __FUNCTION__ << ": 0x" << GetLastError() << "\n";
				MessageBox(nullptr, L"Failed open file.", L"Hook", MB_ICONSTOP);
				exit(-12);
			}
			Sleep(500);
		}
	}
	while (hFile == INVALID_HANDLE_VALUE);

	WriteFile(hFile, reinterpret_cast<LPCVOID>(chunk), sizeof(WavDataChunk), &written, NULL);
	if (written != sizeof(WavDataChunk))
	{
		std::cerr << "Failed write data chunk header. 0x" << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed write data chunk header.", L"Hook", MB_ICONSTOP);
		exit(-13);
	}

	CloseHandle(hFile);
}


static void AppendDataChunk(LPTSTR dumpPath, LPVOID buf, DWORD size)
{
	HANDLE hFile;
	DWORD written;
	INT failCount{ 0 };

	do
	{
		hFile = CreateFile(dumpPath, FILE_APPEND_DATA, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() != ERROR_SHARING_VIOLATION)
			{
				std::cerr << "Failed to open file in " << __FUNCTION__ << ": 0x" << GetLastError() << "\n";
				MessageBox(nullptr, L"Failed open file.", L"Hook", MB_ICONSTOP);
				exit(-12);
			}
			if (++failCount == 10)
			{
				std::cerr << "Failed to open file in " << __FUNCTION__ << ": 0x" << GetLastError() << "\n";
				MessageBox(nullptr, L"Failed open file.", L"Hook", MB_ICONSTOP);
				exit(-12);
			}
			Sleep(500);
		}
	} while (hFile == INVALID_HANDLE_VALUE);

	WriteFile(hFile, buf, size, &written, NULL);
	if (written != size)
	{
		std::cerr << "Failed to append data chunk. " << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed to append data chunk.", L"Hook", MB_ICONSTOP);
		exit(-14);
	}

	CloseHandle(hFile);
}

static void WriteHeader(LPTSTR dumpPath)
{
	WavHeader hdr
	{
		{ 'R', 'I', 'F', 'F'},
		0, // Yet unknown.
		{ 'W', 'A', 'V', 'E' }
	};

	WavFormatChunk fmt
	{
		{ 'f', 'm', 't', ' ' },
		16,
		1,
		2,
		44100,
		44100 * 2 * 2,
		2 * 2,
		16
	};

	HANDLE hFile;
	DWORD written;

	hFile = CreateFile(dumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		std::cerr << "Failed to open file in " << __FUNCTION__ << ": 0x" << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed open new file.", L"Hook", MB_ICONSTOP);
		exit(-12);
	}

	WriteFile(hFile, reinterpret_cast<LPCVOID>(&hdr), sizeof(hdr), &written, NULL);
	if (written != sizeof(hdr))
	{
		std::cerr << "Failed write header. " << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed write header.", L"Hook", MB_ICONSTOP);
		exit(-13);
	}

	WriteFile(hFile, reinterpret_cast<LPCVOID>(&fmt), sizeof(fmt), &written, NULL);
	if (written != sizeof(fmt))
	{
		std::cerr << "Failed write fmt chunk. " << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed write fmt chunk.", L"Hook", MB_ICONSTOP);
		exit(-14);
	}

	CloseHandle(hFile);
}

static void UpdateFileSize(LPTSTR dumpPath)
{
	HANDLE hFile = CreateFile(dumpPath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD written;

	if (hFile == INVALID_HANDLE_VALUE)
	{
		std::cerr << "Failed open new file for file size update. " << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed open new file for file size update.", L"Hook", MB_ICONSTOP);
		exit(-12);
	}
	DWORD size = GetFileSize(hFile, NULL) - 8;

	if (SetFilePointer(hFile, offsetof(WavHeader, dwFileLength), 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		std::cerr << "Failed set seek postion. " << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed set seek postion.", L"Hook", MB_ICONSTOP);
		exit(-12);
	}

	WriteFile(hFile, reinterpret_cast<LPCVOID>(&size), sizeof(size), &written, NULL);
	if (written != sizeof(size))
	{
		std::cerr << "Failed write fmt chunk. " << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed write fmt chunk.", L"Hook", MB_ICONSTOP);
		exit(-14);
	}

	if (SetFilePointer(hFile, sizeof(WavHeader) + sizeof(WavFormatChunk) + offsetof(WavDataChunk, dwChunkSize), 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		std::cerr << "Failed set seek postion. " << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed set seek postion.", L"Hook", MB_ICONSTOP);
		exit(-12);
	}

	WriteFile(hFile, reinterpret_cast<LPCVOID>(&size), sizeof(size), &written, NULL);
	if (written != sizeof(size))
	{
		std::cerr << "Failed write fmt chunk. " << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed write fmt chunk.", L"Hook", MB_ICONSTOP);
		exit(-14);
	}

	CloseHandle(hFile);
}

void DumpSong(LPVOID buf, DWORD size)
{
	if (size == 0)
	{
		return;
	}

	LPTSTR songName = GetCurrentSong();

	if (!songName)
	{
		return;
	}

	size_t songNameSize = 0;
	if (FAILED(StringCbLength(songName, STRSAFE_MAX_CCH * sizeof(TCHAR), &songNameSize)))
	{
		std::cerr << "Failed StringCbLength. " << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed StringCbLength.", L"Hook", MB_ICONSTOP);
		exit(-14);
	}

	size_t dumpPathSize = sizeof(DUMP_DIR) + songNameSize + 4 * sizeof(TCHAR);
	LPTSTR dumpPath = static_cast<LPTSTR>(malloc(dumpPathSize + sizeof(TCHAR)));
	assert(dumpPath);

	if (FAILED(StringCchPrintf(dumpPath, dumpPathSize, DUMP_DIR L"%ws.wav", songName)))
	{
		std::cerr << "Failed StringCchPrintf. " << GetLastError() << "\n";
		MessageBox(nullptr, L"Failed StringCchPrintf.", L"Hook", MB_ICONSTOP);
		exit(-14);
	}

	if (!PathFileExists(dumpPath))
	{
		WriteHeader(dumpPath);
		WriteInitialDataChunk(dumpPath);
	}

	AppendDataChunk(dumpPath, buf, size);

	UpdateFileSize(dumpPath);

	free(songName);
	free(dumpPath);
}
