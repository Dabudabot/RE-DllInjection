// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "detour.h"

bool APIENTRY DllMain(HMODULE hModule, const DWORD ulReasonForCall, LPVOID lpReserved)
{
	switch (ulReasonForCall)
	{
	case DLL_PROCESS_ATTACH:
		MyD3D11CreateDeviceAndSwapChain::hooker.initHook(L"D3D11.dll", "D3D11CreateDeviceAndSwapChain", MyD3D11CreateDeviceAndSwapChain::MyFunction);
		break;
	case DLL_PROCESS_DETACH:
		MyD3D11CreateDeviceAndSwapChain::hooker.unhook();
		break;
	default: break;
	}
	return true;
}
