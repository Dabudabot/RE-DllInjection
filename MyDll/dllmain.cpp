// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		MessageBox(nullptr, L"MyDll process started!", L"MyDll.dll", MB_OK);
		ExitProcess(1);
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
		MessageBox(nullptr, L"MyDll process ended!", L"MyDll.dll", MB_OK);
        break;
    default: 
    	break;
    }
    return TRUE;
}

