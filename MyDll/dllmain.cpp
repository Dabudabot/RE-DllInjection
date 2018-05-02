// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#define SIZE 6

typedef HRESULT(WINAPI *pfn_D3D11CreateDeviceAndSwapChain)(
	_In_opt_        IDXGIAdapter         *pAdapter,
	D3D_DRIVER_TYPE      DriverType,
	HMODULE              Software,
	UINT                 Flags,
	_In_opt_  const D3D_FEATURE_LEVEL    *pFeatureLevels,
	UINT                 FeatureLevels,
	UINT                 SDKVersion,
	_In_opt_  const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	_Out_opt_       IDXGISwapChain       **ppSwapChain,
	_Out_opt_       ID3D11Device         **ppDevice,
	_Out_opt_       D3D_FEATURE_LEVEL    *pFeatureLevel,
	_Out_opt_       ID3D11DeviceContext  **ppImmediateContext
	);
HRESULT MyD3D11CreateDeviceAndSwapChain(
	_In_opt_        IDXGIAdapter         *pAdapter,
	D3D_DRIVER_TYPE      DriverType,
	HMODULE              Software,
	UINT                 Flags,
	_In_opt_  const D3D_FEATURE_LEVEL    *pFeatureLevels,
	UINT                 FeatureLevels,
	UINT                 SDKVersion,
	_In_opt_  const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	_Out_opt_       IDXGISwapChain       **ppSwapChain,
	_Out_opt_       ID3D11Device         **ppDevice,
	_Out_opt_       D3D_FEATURE_LEVEL    *pFeatureLevel,
	_Out_opt_       ID3D11DeviceContext  **ppImmediateContext
);            // Our detour

void BeginRedirect(LPVOID);
pfn_D3D11CreateDeviceAndSwapChain pOrigMBAddress = NULL;                                // address of original
BYTE oldBytes[SIZE] = { 0 };                                         // backup
BYTE JMP[SIZE] = { 0 };                                              // 6 byte JMP instruction
DWORD oldProtect, myProtect = PAGE_EXECUTE_READWRITE;

bool APIENTRY DllMain(HMODULE hModule, const DWORD ulReasonForCall, LPVOID lpReserved)
{
	switch (ulReasonForCall)
	{
	case DLL_PROCESS_ATTACH:
		MessageBox(nullptr, L"... is loading", L"MyDll.dll", MB_OK);
		pOrigMBAddress = (pfn_D3D11CreateDeviceAndSwapChain)
			GetProcAddress(GetModuleHandle(L"D3D11.dll"),               // get address of original 
				"D3D11CreateDeviceAndSwapChain");

		Sleep(2000);

		if (pOrigMBAddress != NULL)
			BeginRedirect(MyD3D11CreateDeviceAndSwapChain);                               // start detouring
		break;
	case DLL_PROCESS_DETACH:
		memcpy(pOrigMBAddress, oldBytes, SIZE);                       // restore backup
		break;
	default: break;
	}
	return true;
}

void BeginRedirect(LPVOID newFunction)
{
	wchar_t text[100];
	wsprintf(text, L"Function %d already hooked", pOrigMBAddress);
	MessageBox(nullptr, text, L"MyDll.dll", MB_OK);

	BYTE tempJMP[SIZE] = { 0xE9, 0x90, 0x90, 0x90, 0x90, 0xC3 };         // 0xE9 = JMP 0x90 = NOP 0xC3 = RET
	memcpy(JMP, tempJMP, SIZE);                                        // store jmp instruction to JMP
	DWORD JMPSize = ((DWORD)newFunction - (DWORD)pOrigMBAddress - 5);  // calculate jump distance
	VirtualProtect((LPVOID)pOrigMBAddress, SIZE,                       // assign read write protection
		PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy(oldBytes, pOrigMBAddress, SIZE);                            // make backup
	memcpy(&JMP[1], &JMPSize, 4);                              // fill the nop's with the jump distance (JMP,distance(4bytes),RET)
	memcpy(pOrigMBAddress, JMP, SIZE);                                 // set jump instruction at the beginning of the original function
	VirtualProtect((LPVOID)pOrigMBAddress, SIZE, oldProtect, NULL);    // reset protection


	MessageBox(nullptr, L"redirect done", L"MyDll.dll", MB_OK);
}

HRESULT MyD3D11CreateDeviceAndSwapChain(
	_In_opt_        IDXGIAdapter         *pAdapter,
	D3D_DRIVER_TYPE      DriverType,
	HMODULE              Software,
	UINT                 Flags,
	_In_opt_  const D3D_FEATURE_LEVEL    *pFeatureLevels,
	UINT                 FeatureLevels,
	UINT                 SDKVersion,
	_In_opt_  const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	_Out_opt_       IDXGISwapChain       **ppSwapChain,
	_Out_opt_       ID3D11Device         **ppDevice,
	_Out_opt_       D3D_FEATURE_LEVEL    *pFeatureLevel,
	_Out_opt_       ID3D11DeviceContext  **ppImmediateContext
)
{
	VirtualProtect((LPVOID)pOrigMBAddress, SIZE, myProtect, NULL);     // assign read write protection
	memcpy(pOrigMBAddress, oldBytes, SIZE);                            // restore backup
	const auto retValue = ::D3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software,
		Flags, pFeatureLevels, FeatureLevels, SDKVersion,
		pSwapChainDesc, ppSwapChain, ppDevice,
		pFeatureLevel, ppImmediateContext);       // get return value of original function
	memcpy(pOrigMBAddress, JMP, SIZE);                                 // set the jump instruction again
	VirtualProtect((LPVOID)pOrigMBAddress, SIZE, oldProtect, NULL);    // reset protection



	MessageBox(nullptr, L"hook function called", L"MyDll.dll", MB_OK);
	return retValue;                                                   // return original return value
}
