#pragma once
#include "stdafx.h"
#include "injector.h"

class Injector86 : public Injector
{
	// Constructors
public:
	Injector86(_In_ STARTUPINFO startupInfo, _In_ PROCESS_INFORMATION processInfo);

	// Methods
public:

	// Overrides
public:
	void		doInjection(_In_ LPCTSTR dllHome) override;

	// Attributes
public:
	UCHAR g_shellcode_x86[48] = //TODO why it requires size?
	{
		/*0x00:*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	//pLoadLibrary pointer, RUNTIME
		/*0x08:*/ 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	//pString pointer, RUNTIME
		/*0x10:*/ 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,	//8x nops to fix disassembly of VS
		/*0x18:*/ 0x68, 0xF0, 0xFF, 0xFF, 0xFF, 					//push        offset string L"kernel32.dll" (0D82108h)
		/*0x1D:*/ 0xFF, 0x15, 0xDD, 0xFF, 0xFF, 0xFF,				//call        dword ptr[g_pLoadLibraryW(0D833BCh)]
		/*0x23:*/ 0xF7, 0xD8,										//neg         eax
		/*0x25:*/ 0x1B, 0xC0,										//sbb         eax, eax
		/*0x27:*/ 0xF7, 0xD8,										//neg         eax
		/*0x29:*/ 0x48,												//dec         eax
		/*0x2A:*/ 0xC3,												//ret
		/*0x2B:*/ 0x90, 0x90, 0x90, 0x90, 0x90						//5x nop for alignment
		/*0x30:*/													//String: "MyDll.dll"
	};
};
