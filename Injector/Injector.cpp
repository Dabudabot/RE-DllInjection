// Injector.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "injector.h"

bool InjectDll(const HANDLE hProcess, const LPCTSTR lpFileName, const PVOID pfnLoadLibrary)
{
	auto ret = false;
	PVOID lp_file_name_remote = nullptr;
	HANDLE h_remote_thread = nullptr;

	for (;;)
	{
		//allocate remote storage
		const DWORD lp_file_name_size = (wcslen(lpFileName) + 1) * sizeof(wchar_t);
		lp_file_name_remote = VirtualAllocEx(hProcess, nullptr, lp_file_name_size, 
			MEM_COMMIT, PAGE_READWRITE);
		if (nullptr == lp_file_name_remote)
		{
			printf("VirtualAllocEx returns NULL \n");
			break;
		}

		//fill remote storage with data
		SIZE_T bytes_written;
		const auto res = WriteProcessMemory(hProcess, lp_file_name_remote, 
			lpFileName, lp_file_name_size, &bytes_written);
		if (false == res)
		{
			printf("WriteProcessMemory failed with %d \n", GetLastError());
			break;
		}

		DWORD tid;
		//in case of problems try MyLoadLibrary if this is actually current process
		h_remote_thread = CreateRemoteThread(hProcess, nullptr, 0, 
			LPTHREAD_START_ROUTINE(pfnLoadLibrary), lp_file_name_remote, 0, &tid);
		if (nullptr == h_remote_thread)
		{
			printf("CreateRemoteThread failed with %d \n", GetLastError());
			break;
		}

		//wait for MyDll initialization
		WaitForSingleObject(h_remote_thread, INFINITE);
		//FUTURE obtain exit code and check

		ret = TRUE;
		break;
	}

	if (!ret)
	{
		if (lp_file_name_remote) SIC_MARK;
		//TODO call VirtualFree(...)
	}

	if (h_remote_thread) CloseHandle(h_remote_thread);
	return ret;
}

//returns entry point in remote process
ULONG_PTR GetEnryPoint(REMOTE_ARGS_DEFS)
{
	return 0;
}

//returns address of LoadLibraryA in remote process
ULONG_PTR GetRemoteLoadLibraryA(REMOTE_ARGS_DEFS)
{
	return 0;
}


bool FindEntryPoint(REMOTE_ARGS_DEFS, const PVOID context)
{
	bool is64;
	const auto p_local_pe_header = GetLocalPeHeader(REMOTE_ARGS_CALL, &is64);
	ULONG_PTR p_remote_entry_point;
	const auto my_context = pentry_point_context(context);

	if (is64)
	{
		const auto p_local_pe_header2 = PIMAGE_NT_HEADERS64(p_local_pe_header);
		p_remote_entry_point = RVA_TO_REMOTE_VA(PVOID,
			p_local_pe_header2->OptionalHeader.AddressOfEntryPoint);
	}
	else
	{
		const auto p_local_pe_header2 = PIMAGE_NT_HEADERS32(p_local_pe_header);
		p_remote_entry_point = RVA_TO_REMOTE_VA(PVOID,
			p_local_pe_header2->OptionalHeader.AddressOfEntryPoint);
	}
	free(p_local_pe_header);

	my_context->remote_remote_entry_point = p_remote_entry_point;

	return false;
}

ULONG_PTR FindEntryPoint2(REMOTE_ARGS_DEFS)
{
	bool is64;
	const auto p_local_pe_header = GetLocalPeHeader(REMOTE_ARGS_CALL, &is64);
	ULONG_PTR p_remote_entry_point;

	if (is64)
	{
		const auto p_local_pe_header2 = PIMAGE_NT_HEADERS64(p_local_pe_header);
		p_remote_entry_point = RVA_TO_REMOTE_VA(
			PVOID,
			p_local_pe_header2->OptionalHeader.AddressOfEntryPoint);
	}
	else
	{
		const auto p_local_pe_header2 = PIMAGE_NT_HEADERS32(p_local_pe_header);
		p_remote_entry_point = RVA_TO_REMOTE_VA(
			PVOID,
			p_local_pe_header2->OptionalHeader.AddressOfEntryPoint);
	}
	free(p_local_pe_header);

	return p_remote_entry_point;
}

int _tmain(int argc, _TCHAR* argv[])
{
	STARTUPINFO info = { sizeof(info) };
	PROCESS_INFORMATION process_info;
	::CreateProcess(L"E:\\Programs\\DirectXSDK\\Samples\\C++\\Direct3D11\\Bin\\x64\\HDRToneMappingCS11.exe", nullptr,
		nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr, nullptr, &info, &process_info);

	Sleep(1000);
	
	//-------
	process_basic_information pbi;
	memset(&pbi, 0, sizeof(pbi));
	DWORD retlen = 0;
	auto status = ZwQueryInformationProcess(
		process_info.hProcess, 
		0, 
		&pbi,
		sizeof(pbi),
		&retlen);

	const auto h_process = process_info.hProcess;
	const auto p_local_peb = REMOTE(peb, ULONG_PTR(pbi.peb_base_address));

	printf("\n");
	printf("from PEB: %p and %p\n", p_local_peb->reserved3[0], p_local_peb->reserved3[1]);

	const auto peb_remote_image_base = ULONG_PTR(p_local_peb->reserved3[1]); // TODO x64 POC only
	const auto p_remote_entry_point = FindEntryPoint2(h_process, peb_remote_image_base);

	WORD orig_word;
	WORD pathed_word = 0xFEEB;
	ReadRemoteDataType<WORD>(h_process, p_remote_entry_point, &orig_word);
	WriteRemoteDataType<WORD>(h_process, p_remote_entry_point, &pathed_word);

	ResumeThread(process_info.hThread); //resumed pathed process
	Sleep(1000);

	DWORD n_modules;
	const auto ph_modules = GetRemoteModules(process_info.hProcess, &n_modules);
	export_context my_context;
	my_context.module_name = "KERNEL32.dll";
	my_context.function_name = "LoadLibraryW";
	my_context.remote_function_address = 0;
	RemoteModuleWorker(process_info.hProcess, ph_modules, n_modules, FindExport, &my_context);
	printf("kernel32!LoadLibraryA is at %llu \n", my_context.remote_function_address);

	entry_point_context my_context2;
	my_context2.remote_remote_entry_point = 0;
	RemoteModuleWorker(process_info.hProcess, ph_modules, n_modules, FindEntryPoint, &my_context2);
	printf("notepad entry pount is at %llu \n", my_context2.remote_remote_entry_point);

	InjectDll(h_process, L"E:\\Documents\\Visual Studio 2017\\Projects\\RE-S18\\x64\\Debug\\MyDll.dll", 
		PVOID(my_context.remote_function_address));
	Sleep(2000);

	status = ZwSuspendProcess(h_process);
	WriteRemoteDataType<WORD>(h_process, p_remote_entry_point, &orig_word);
	status = ZwResumeProcess(h_process);
	Sleep(10000);
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