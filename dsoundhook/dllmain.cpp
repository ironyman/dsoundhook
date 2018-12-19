// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "log.h"
#include "my_exception.h"
#include <iostream>

static redirect_cerr* log_raii = nullptr;

#if 0
DetourCreateProcessWithDllEx modifies the in - memory import table of the target PE binary program in the new 
process it creates.The updated import table will contain a reference to function ordinal #1 exported from the 
target DLL.If the target process is 32 - bit and the parent process is 64 - bit, or if the target process is 64 
- bit and the parent process is 32 - bit, DetourCreateProcessWithDllEx will use rundll32.exe to load the DLL into 
a helper process that matches the target process temporarily in order to update the target processes import table
with the correct DLL.

Note: The new process will fail to start if the target DLL does not contain a exported function with ordinal #1.
#endif
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllexport) int __stdcall placeholder(void)
{
	return 0;
}
#ifdef __cplusplus 
}
#endif



BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		try
		{
			log_raii = new redirect_cerr(".\\dsoundhook.txt");
		}
		catch (std::exception e)
		{
			const char* s = e.what();
			size_t s_len = strlen(s);
			int cchWideChar = MultiByteToWideChar(CP_UTF8, 0, s, s_len, NULL, 0);
			WCHAR* ws = new WCHAR[cchWideChar + 1];
			MultiByteToWideChar(CP_UTF8, 0, s, s_len, ws, cchWideChar);
			ws[cchWideChar] = '\0';

			MessageBox(NULL, ws, L"DSound hook", 0);
			delete ws;
			return false;
		}
		
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SetupThread, NULL, 0L, NULL);
		break;
		/*try
		{
			SetupHook();
		}
		catch (my_exception e)
		{
			MessageBox(0, L"HI", L"HI", 0);
			std::cerr << e.wwhat();
			return false;
		}
		std::cerr << "Finished setup.\n";
		break;*/
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
		break;
    case DLL_PROCESS_DETACH:
		if (log_raii)
		{
			delete log_raii;
		}
		CleanHook();
        break;
    }
    return TRUE;
}

