// injector.cpp : injector realization
#include "stdafx.h"
#include "injector.h"

Injector::Injector(const STARTUPINFO startupInfo, const PROCESS_INFORMATION processInfo)
{
	m_startupInfo = startupInfo;
	m_processInfo = processInfo;
	m_hProcess = processInfo.hProcess;

	m_loadLibraryContext.m_moduleName = "KERNEL32.dll";
	m_loadLibraryContext.m_functionName = "LoadLibraryW";
	m_loadLibraryContext.m_remoteFunctionAddress = 0;
}

HMODULE* Injector::getRemoteModules(const HANDLE hProcess, DWORD* pnModules)
{
	DWORD cb_needed, cb = 1 * sizeof(HMODULE);
	HMODULE* ph_modules = nullptr;
	for (;;)
	{
		ph_modules = static_cast<HMODULE*>(malloc(cb));
		const auto res = EnumProcessModulesEx(
			hProcess,
			ph_modules,
			cb,
			&cb_needed,
			LIST_MODULES_ALL);
		if (res == 0)
		{
			printf("EnumProcessModulesEx returns %d, cbNeeded is %d \n",
				GetLastError(), cb_needed);
			free(ph_modules);
			return nullptr;
		}

		if (cb == cb_needed)
		{
			printf("Success, cbNeeded is %d \n", cb_needed);
			break;
		}

		free(ph_modules);
		cb = cb_needed;
		continue;
	}

	*pnModules = cb / sizeof(HMODULE);
	return ph_modules;
}

PROCESS_BASIC_INFORMATION Injector::getProcessBasicInformation(const HANDLE hProcess)
{
	PROCESS_BASIC_INFORMATION pbi;
	ZeroMemory(&pbi, sizeof(pbi));
	DWORD retlen = 0;
	//it returns wrong pbi for 86
	const auto status = ZwQueryInformationProcess(
		hProcess,
		0,
		&pbi,
		sizeof(pbi),
		&retlen);

	if (status)
	{
		printf("ZwQueryInformationProcess failed with %x\n", status);
		printf("%s failed\n", "getRemoteImageBase");
	}
	return pbi;
}

bool Injector::loopEntryPoint(const HANDLE hProcess, const HANDLE hThread, const ULONG_PTR remoteEntryPoint, WORD* originalEntryPointValue)
{
	WORD patchedEntryPoint = 0xFEEB;
	Remote::readRemoteDataType<WORD>(hProcess, remoteEntryPoint, originalEntryPointValue);
	Remote::writeRemoteDataType<WORD>(hProcess, remoteEntryPoint, &patchedEntryPoint);

	ResumeThread(hThread); //resumed pathed process
	Sleep(1000);

	printf("%s OK\n", "loopEntryPoint");
	return true;
}

bool Injector::deLoopEntryPoint(const HANDLE hProcess, const ULONG_PTR remoteEntryPoint, WORD* originalEntryPointValue)
{
	auto status = ZwSuspendProcess(hProcess);
	if (status)
	{
		printf("ZwSuspendProcess failed with %x\n", status);
		printf("%s failed\n", "deLoopEntryPoint");
		return false;
	}

	Remote::writeRemoteDataType<WORD>(hProcess, remoteEntryPoint, originalEntryPointValue);
	status = ZwResumeProcess(hProcess);
	if (status)
	{
		printf("ZwResumeProcess failed with %x\n", status);
		printf("%s failed\n", "deLoopEntryPoint");
		return false;
	}
	Sleep(1000);

	printf("%s OK\n", "deLoopEntryPoint");
	return true;
}

bool Injector::inject(const HANDLE hProcess, const LPCTSTR lpDllName, const UCHAR* shellcode, const SIZE_T szShellcode, FunctionContext context)
{
	auto ret = false;
	PVOID lp_shellcodeRemote = nullptr;
	HANDLE h_remoteThread = nullptr;
	const auto lp_dllNameSize = DWORD((wcslen(lpDllName) + 1) * sizeof(wchar_t));
	const DWORD lp_shellcodeSize = szShellcode + lp_dllNameSize;

	for (;;)
	{
		//allocate remote storage
		lp_shellcodeRemote = VirtualAllocEx(hProcess, nullptr, lp_shellcodeSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (nullptr == lp_shellcodeRemote)
		{
			printf("VirtualAllocEx returns NULL \n");
			break;
		}
		const auto dp_shellcodeRemote = DWORD_PTR(lp_shellcodeRemote);

		//fill remote storage with actual shellcode
		SIZE_T bytesWritten;
		auto res = WriteProcessMemory(hProcess,
			lp_shellcodeRemote,
			shellcode,
			szShellcode,
			&bytesWritten);

		if (false == res)
		{
			printf("WriteProcessMemory failed with %d \n", GetLastError());
			break;
		}

		//fill remote storage with string
		res = WriteProcessMemory(hProcess,
			rvaToVa<PVOID>(dp_shellcodeRemote, szShellcode),
			lpDllName,
			lp_dllNameSize,
			&bytesWritten);

		if (false == res)
		{
			printf("WriteProcessMemory failed with %d \n", GetLastError());
			break;
		}

		//adjust pfnLoadLibrary
		const DWORD patchedPointerRva = 0x00;
		Remote::writeRemoteDataType<ULONG_PTR>(hProcess,
			rvaToVa<ULONG_PTR>(dp_shellcodeRemote, patchedPointerRva),
			&context.m_remoteFunctionAddress);

		DWORD tid;
		//in case of problems try MyLoadLibrary if this is actually current process
		h_remoteThread = CreateRemoteThread(hProcess,
			nullptr,
			0,
			LPTHREAD_START_ROUTINE(rvaToVa<ULONG_PTR>(dp_shellcodeRemote, 0x10)),
			lp_shellcodeRemote,
			0,
			&tid);
		if (nullptr == h_remoteThread)
		{
			printf("CreateRemoteThread failed with %d \n", GetLastError());
			break;
		}

		//wait for MyDll initialization
		WaitForSingleObject(h_remoteThread, INFINITE);

		DWORD exit_code = 0xDEADFACE;
		GetExitCodeThread(h_remoteThread, &exit_code);
		printf("GetExitCodeThread returns %d\n", exit_code);

		ret = true;
		break;
	}

	if (!ret)
	{
		if (lp_shellcodeRemote)
		{
			VirtualFreeEx(hProcess, nullptr, lp_shellcodeSize, MEM_DECOMMIT);
		}
	}

	if (h_remoteThread) CloseHandle(h_remoteThread);
	return ret;
}
