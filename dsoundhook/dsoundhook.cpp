#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cassert>
#include <mmsystem.h>
#include <dsound.h>

#include "my_exception.h"

// msg is string or at least convertible to string pls.
#define hr_throw(hr, msg) throw my_exception(std::to_string(hr) + " " + msg, __FILE__, __LINE__, __FUNCTION__);

#define throw_if_failed(hr, msg) do { \
	if (FAILED(hr)) \
	{ \
		hr_throw(hr, msg); \
	} \
} while (0)

// See README.txt for how to build these. They're not built by the solution.
#include "..\\Detours\\include\\detours.h"
#pragma comment(lib, "..\\Detours\\lib.X86\\detours.lib")

typedef HRESULT (WINAPI* DirectSoundCreateProc)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
typedef HRESULT (__stdcall* UnlockProc)(
	IDirectSoundBuffer* self,
	LPVOID pvAudioPtr1,
	DWORD dwAudioBytes1,
	LPVOID pvAudioPtr2,
	DWORD dwAudioBytes2);

static UnlockProc pUnlockOrig = nullptr;

HMODULE LoadDsound()
{
	wchar_t systemPath[MAX_PATH];

	if (GetSystemDirectory(systemPath, MAX_PATH) == 0)
	{
		my_throw("Failed to get system directory.");
	}

	std::wstring dsoundPath(systemPath);
	dsoundPath += L"\\dsound.dll";

	std::cerr << "Loading "
			  << std::string(dsoundPath.begin(), dsoundPath.end())
		      << "\n";

	return LoadLibrary(dsoundPath.c_str());
}

HRESULT __stdcall MyUnlockProc(
	IDirectSoundBuffer* self,
	LPVOID pvAudioPtr1,
	DWORD dwAudioBytes1,
	LPVOID pvAudioPtr2,
	DWORD dwAudioBytes2)
{
	/*std::cerr << "MyUnlockProc entry "
		<< self << " "
		<< pvAudioPtr1 << " "
		<< dwAudioBytes1 << " "
		<< pvAudioPtr2 << " "
		<< dwAudioBytes2 << "\n";*/
	/*MessageBox(0, L"IH", L"HI", 0);*/

	DumpSong(pvAudioPtr1, dwAudioBytes1);
	// dwAudioBytes2 == 0 if write to ring buffer didn't wrap.
	DumpSong(pvAudioPtr2, dwAudioBytes2);

	HRESULT hr = pUnlockOrig(self, pvAudioPtr1, dwAudioBytes1, pvAudioPtr2, dwAudioBytes2);
	
	// std::cerr << "MyUnlockProc exit\n";
	return hr;
}

// Disassemble me!
void DummyGetVtableOffset()
{
#if 0
	sub_10031570    proc near; CODE XREF : sub_10011E65↑j
		.text : 10031570
		.text : 10031570 var_C0 = byte ptr - 0C0h
		.text : 10031570
		.text : 10031570                 push    ebp
		.text : 10031571                 mov     ebp, esp
		.text : 10031573                 sub     esp, 0C0h
		.text : 10031579                 push    ebx
		.text : 1003157A                 push    esi
		.text : 1003157B                 push    edi
		.text : 1003157C                 lea     edi, [ebp + var_C0]
		.text : 10031582                 mov     ecx, 30h
		.text : 10031587                 mov     eax, 0CCCCCCCCh
		.text : 1003158C                 rep stosd
		.text : 1003158E                 mov     ecx, offset unk_10042043
		.text : 10031593                 call    sub_10011893
		.text : 10031598                 mov     esi, esp
		.text : 1003159A                 push    3
		.text : 1003159C                 push    2
		.text : 1003159E                 push    1
		.text : 100315A0                 push    0
		.text : 100315A2                 mov     eax, dword_1003E6D8
		.text : 100315A7                 mov     ecx, [eax]
		.text : 100315A9                 mov     edx, dword_1003E6D8
		.text : 100315AF                 push    edx
		.text : 100315B0                 mov     eax, [ecx + 4Ch] Im guessing  0x4c is the offset
		.text : 100315B3                 call    eax
		.text : 100315B5                 cmp     esi, esp
		.text : 100315B7                 call    sub_100118C5
		.text : 100315BC                 pop     edi
		.text : 100315BD                 pop     esi
		.text : 100315BE                 pop     ebx
		.text : 100315BF                 add     esp, 0C0h
		.text : 100315C5                 cmp     ebp, esp
		.text : 100315C7                 call    sub_100118C5
		.text : 100315CC                 mov     esp, ebp
		.text : 100315CE                 pop     ebp
		.text : 100315CF                 retn
		.text : 100315CF sub_10031570    endp
#endif
	IDirectSoundBuffer* dsbuffer = reinterpret_cast<IDirectSoundBuffer*>(0xee);
	dsbuffer->Unlock(NULL, 1, (void *)2, 3);
}

void SetupHook()
{
	DirectSoundCreateProc pDirectSoundCreate = nullptr;
	IDirectSound* directSound = nullptr;
	IDirectSoundBuffer* dsbuffer = nullptr;

	pDirectSoundCreate = (DirectSoundCreateProc)GetProcAddress(LoadDsound(), "DirectSoundCreate");
	std::cerr << "Got DirectSoundCreate " << std::hex << pDirectSoundCreate << "\n";

	HRESULT hr = pDirectSoundCreate(NULL, &directSound, NULL);
	throw_if_failed(hr, "Failed to create direct sound device.\n");
	std::cerr << "Got direct sound device " << directSound << "\n";

	// Have to do this before creating buffer?
	directSound->SetCooperativeLevel(GetDesktopWindow(), DSSCL_NORMAL);

	WAVEFORMATEX wfmt = { 0 };
	wfmt.wFormatTag = 1;
	wfmt.nChannels = 2;
	wfmt.nSamplesPerSec = 44100;
	wfmt.wBitsPerSample = 24;
	wfmt.nBlockAlign = wfmt.wBitsPerSample*wfmt.nChannels / 8;
	wfmt.nAvgBytesPerSec = wfmt.nSamplesPerSec * wfmt.nBlockAlign;
	wfmt.cbSize = 12;

	DSBUFFERDESC bufferDesc = { sizeof(DSBUFFERDESC), 0, DSBSIZE_MIN, 0, &wfmt, GUID_NULL };

	hr = directSound->CreateSoundBuffer(&bufferDesc, &dsbuffer, NULL);
	throw_if_failed(hr, "Failed to create direct sound buffer.\n");

	std::cerr << "Got direct sound buffer " << dsbuffer << "\n";

	void **vtable = *reinterpret_cast<void ***>(dsbuffer);
	// BTW 0x4c / 4 is 19, which I could have got from counting the methods in definition.
	pUnlockOrig = reinterpret_cast<UnlockProc>(vtable[0x4c / sizeof(void *)]);

	std::cerr << "Got vtable " << vtable << "\n";
	std::cerr << "Guessing Unlock is here " << pUnlockOrig << "\n";
	std::cerr << "Doing the hook!\n";

	DetourRestoreAfterWith();

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)pUnlockOrig, MyUnlockProc);
	DetourTransactionCommit();

	std::cout.copyfmt(std::ios(nullptr));
/*
	if (dsbuffer)
	{
		dsbuffer->Release();
		dsbuffer = nullptr;
	}

	if (directSound)
	{
		directSound->Release();
		directSound = nullptr;
	}*/
}

// CreateDirectSound hangs when called from DllMain, so use another thread.
DWORD SetupThread(LPVOID param)
{
	// Just look for CreateThread in disassebmly to find this guy.
	// DummyGetVtableOffset();
	std::cerr << "Setup thread!\n";
	try
	{
		SetupHook();
	}
	catch (my_exception e)
	{
		std::cerr << e.what();
		return -1;
	}
	std::cerr << "Finished setup.\n";
	return 0;
}

void CleanHook()
{
	if (pUnlockOrig)
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(PVOID&)pUnlockOrig, MyUnlockProc);
		DetourTransactionCommit();
	}
}