// Injector.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "remote.h"
#define SIC_MARK

HMODULE WINAPI MyLoadLibrary(
	_In_ LPCTSTR lpFileName
)
{
	return ::LoadLibrary(lpFileName);
}

BOOL InjectDll(HANDLE hProcess, LPCTSTR lpFileName, PVOID pfnLoadLibrary)
{
	BOOL ret = FALSE;
	PVOID lpFileName_remote = NULL;
	HANDLE hRemoteThread = NULL;

	for (;;)
	{
		//allocate remote storage
		DWORD lpFileName_size = (wcslen(lpFileName) + 1) * sizeof(wchar_t);
		lpFileName_remote = VirtualAllocEx(hProcess, NULL,
			lpFileName_size, MEM_COMMIT, PAGE_READWRITE);
		if (NULL == lpFileName_remote)
		{
			printf("VirtualAllocEx returns NULL \n");
			break;
		}

		//fill remote storage with data
		SIZE_T bytesWritten;
		BOOL res = WriteProcessMemory(hProcess, lpFileName_remote,
			lpFileName, lpFileName_size, &bytesWritten);
		if (FALSE == res)
		{
			printf("WriteProcessMemory failed with %d \n", GetLastError());
			break;
		}

		DWORD tid;
		//in case of problems try MyLoadLibrary if this is actually current process
		hRemoteThread = CreateRemoteThread(hProcess,
			NULL, 0, (LPTHREAD_START_ROUTINE)pfnLoadLibrary, lpFileName_remote,
			0, &tid);
		if (NULL == hRemoteThread)
		{
			printf("CreateRemoteThread failed with %d \n", GetLastError());
			break;
		}

		//wait for MyDll initialization
		WaitForSingleObject(hRemoteThread, INFINITE);
		//FUTURE obtain exit code and check

		ret = TRUE;
		break;
	}

	if (!ret)
	{
		if (lpFileName_remote) SIC_MARK;
		//TODO call VirtualFree(...)
	}

	if (hRemoteThread) CloseHandle(hRemoteThread);
	return ret;
}

void PrintExportTable()
{
}

PVOID GetExportTable(HANDLE hProcess, ULONG_PTR pRemoteImageBase)
{
	PIMAGE_DOS_HEADER pLocalDosHeader = REMOTE(IMAGE_DOS_HEADER, pRemoteImageBase);
	//free(pLocalDosHeader);

	ULONG_PTR pRemotePeHeader =
		RVA_TO_REMOTE_VA(PIMAGE_NT_HEADERS, pRemoteImageBase, pLocalDosHeader->e_lfanew);

	PIMAGE_NT_HEADERS pLocalPeHeader = REMOTE(IMAGE_NT_HEADERS, pRemotePeHeader);

	//check PE signature
	DWORD Signature = pLocalPeHeader->Signature;
	BYTE sig0 = ((BYTE*)&Signature)[0];
	BYTE sig1 = ((BYTE*)&Signature)[1];
	BYTE sig2 = ((BYTE*)&Signature)[2];
	BYTE sig3 = ((BYTE*)&Signature)[3];
	printf("PeHeader starts with %c%c%d%d \n", sig0, sig1, sig2, sig3);

	BYTE* sig = REMOTE_ARRAY_FIXED(BYTE, pRemotePeHeader, 4);
	printf("PeHeader starts with %c%c%d%d \n", sig[0], sig[1], sig[2], sig[3]);

	DWORD n;
	BYTE* sig42 = REMOTE_ARRAY_ZEROENDED(BYTE, pRemotePeHeader, n);
	printf("PeHeader starts with %s (strlen=%d) \n", sig42, n);

	BOOL is64 = FALSE;
	switch (pLocalPeHeader->FileHeader.Machine)
	{
	case IMAGE_FILE_MACHINE_I386:
		assert(sizeof(IMAGE_OPTIONAL_HEADER32) ==
			pLocalPeHeader->FileHeader.SizeOfOptionalHeader);
		break;
	case IMAGE_FILE_MACHINE_AMD64:
		is64 = TRUE;
		assert(sizeof(IMAGE_OPTIONAL_HEADER64) ==
			pLocalPeHeader->FileHeader.SizeOfOptionalHeader);
		break;
	default:
		printf("Unsupported hardware platfrom, Machine is %d",
			pLocalPeHeader->FileHeader.Machine);
		return nullptr;
	}

	if (is64)
	{
		PIMAGE_NT_HEADERS64 pLocalPeHeader2 = REMOTE(IMAGE_NT_HEADERS64, pRemotePeHeader);

		ULONG_PTR pRemoteImageImportDescriptor = RVA_TO_REMOTE_VA(
			PIMAGE_IMPORT_DESCRIPTOR, pRemoteImageBase,
			pLocalPeHeader2->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

		DWORD nDescs = 0;
		PIMAGE_IMPORT_DESCRIPTOR pLocalImageImportDescriptor =
			REMOTE_ARRAY_ZEROENDED(IMAGE_IMPORT_DESCRIPTOR, pRemoteImageImportDescriptor, nDescs);

		for (DWORD i = 0; i < nDescs; i++)
		{
			ULONG_PTR pRemoteDllName = RVA_TO_REMOTE_VA(
				char*, pRemoteImageBase,
				pLocalImageImportDescriptor[i].Name);

			char* pLocalDllName = REMOTE_ARRAY_ZEROENDED_NOLEN(char, pRemoteDllName);
			printf("Imports from %s \n", pLocalDllName);
		}
	}
	else
	{
		PIMAGE_NT_HEADERS32 pLocalPeHeader2 = REMOTE(IMAGE_NT_HEADERS32, pRemotePeHeader);

		ULONG_PTR pRemoteImageImportDescriptor = RVA_TO_REMOTE_VA(
			PIMAGE_IMPORT_DESCRIPTOR, pRemoteImageBase,
			pLocalPeHeader2->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

		PIMAGE_IMPORT_DESCRIPTOR pLocalImageImportDescriptor = REMOTE(IMAGE_IMPORT_DESCRIPTOR, pRemoteImageImportDescriptor);
	}
	return nullptr;
}

void ModuleWorker(HANDLE hProcess, int i, HMODULE module)
{
	printf("module %d at %p \n", i, module);
	GetExportTable(hProcess, ULONG_PTR(module));
}

int _tmain(int argc, _TCHAR* argv[])
{
	STARTUPINFO info = { sizeof(info) };
	PROCESS_INFORMATION processInfo;
	::CreateProcess(L"C:\\Windows\\system32\\notepad.exe", NULL,
		NULL, NULL, TRUE, 0 /*CREATE_SUSPENDED*/, NULL, NULL, &info, &processInfo);

	Sleep(1000);

	HANDLE hProcess = processInfo.hProcess;
	DWORD cbNeeded, cb = 1 * sizeof(HMODULE);
	HMODULE* phModules;
	for (;;)
	{
		phModules = (HMODULE*)malloc(cb);
		BOOL res = EnumProcessModulesEx(
			hProcess,
			phModules,
			cb,
			&cbNeeded,
			LIST_MODULES_ALL);
		if (res == 0)
		{
			printf("EnumProcessModulesEx returns %d, cbNeeded is %d \n",
				GetLastError(), cbNeeded);
			break;
		}

		if (cb == cbNeeded)
		{
			printf("Success, cbNeeded is %d \n", cbNeeded);
			break;
		}

		free(phModules);
		cb = cbNeeded;
		continue;
	}

	for (int i = 0; i < cb / sizeof(HMODULE); i++)
	{
		ModuleWorker(hProcess, i, phModules[i]);
	}

#if 0
	//magic to calculate remote LoadLibraryW
	//if you'll manually adjust pKernel32base_remote via Debugger
	HMODULE pKernel32base = GetModuleHandle(L"kernel32.dll");
	PVOID pLoadLibraryW = GetProcAddress(pKernel32base, "LoadLibraryW");
	DWORD_PTR ofs_LoadLibrary =
		((DWORD_PTR)pLoadLibraryW) - ((DWORD_PTR)pKernel32base);
	HMODULE pKernel32base_remote = NULL;
	PVOID pLoadLibraryW_remote =
		(PVOID)((DWORD_PTR(pKernel32base_remote)) + ofs_LoadLibrary);
#endif

#if 0
	//HANDLE hProcess = GetCurrentProcess();
	HANDLE hProcess = processInfo.hProcess;
	InjectDll(hProcess, L"D:\\PROJECTS\\Inno-S18-RevEng-DllInjection\\Debug\\MyDll.dll",
#if 0
		pLoadLibraryW_remote);
#else
		LoadLibrary);
#endif
	Sleep(10000);
#endif

#if 0
	LPCTSTR cstr = L"MyDll.dll";
	DWORD cstr_sz = (wcslen(cstr) + 1) * sizeof(wchar_t);
	DWORD tid;

	PVOID pstr = VirtualAlloc(NULL, cstr_sz, MEM_COMMIT, PAGE_READWRITE);
	memcpy(pstr, cstr, cstr_sz);
	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibrary, pstr, 0, &tid);
	if (hThread == NULL)
	{
		DWORD err = GetLastError();
		printf("CreateThread failed with %d \n", err);
	}
#endif	

#if 0
	::LoadLibrary(L"MyDll.dll");
#endif

	/*
	WaitForSingleObject(processInfo.hProcess, INFINITE);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
	*/
	return 0;
}

