// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>


void SetupHook();
void CleanHook();
DWORD SetupThread(LPVOID param);
void DumpSong(LPVOID buf, DWORD size);

// reference additional headers your program requires here
