// Injector.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "injector.h"

#if 0


bool InjectDll(const HANDLE hProcess, const LPCTSTR lpFileName, PVOID pfnLoadLibrary)
{
	auto ret = false;
	PVOID lp_shellcode_remote = nullptr;
	HANDLE h_remote_thread = nullptr;

	for (;;)
	{
		//allocate remote storage
		const DWORD lp_file_name_size = (wcslen(lpFileName) + 1) * sizeof(wchar_t);
		const DWORD lp_shellcode_size = sizeof(g_shellcode_x64) + lp_file_name_size;
		lp_shellcode_remote = VirtualAllocEx(hProcess, nullptr,
			lp_shellcode_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (nullptr == lp_shellcode_remote)
		{
			printf("VirtualAllocEx returns NULL \n");
			break;
		}

		//fill remote storage with actual shellcode
		SIZE_T bytes_written;
		auto res = WriteProcessMemory(hProcess, lp_shellcode_remote,
			g_shellcode_x64, sizeof(g_shellcode_x64), &bytes_written);
		if (false == res)
		{
			printf("WriteProcessMemory failed with %d \n", GetLastError());
			break;
		}

		//fill remote storage with string
		res = WriteProcessMemory(hProcess, RVA_TO_VA(PVOID, lp_shellcode_remote, sizeof(g_shellcode_x64)),
			lpFileName, lp_file_name_size, &bytes_written);
		if (false == res)
		{
			printf("WriteProcessMemory failed with %d \n", GetLastError());
			break;
		}

		//adjust pfnLoadLibrary
		const DWORD patched_pointer_rva = 0x00;
		auto patched_pointer_value = ULONG_PTR(pfnLoadLibrary);
		WriteRemoteDataType<ULONG_PTR>(hProcess,
			RVA_TO_VA(ULONG_PTR, lp_shellcode_remote, patched_pointer_rva),
			&patched_pointer_value);

		DWORD tid;
		//in case of problems try MyLoadLibrary if this is actually current process
		h_remote_thread = CreateRemoteThread(hProcess,
			nullptr, 0, LPTHREAD_START_ROUTINE(RVA_TO_VA(ULONG_PTR, lp_shellcode_remote, 0x10)),
			lp_shellcode_remote,
			0, &tid);
		if (nullptr == h_remote_thread)
		{
			printf("CreateRemoteThread failed with %d \n", GetLastError());
			break;
		}

		//wait for MyDll initialization
		WaitForSingleObject(h_remote_thread, INFINITE);

		DWORD exit_code = 0xDEADFACE;
		GetExitCodeThread(h_remote_thread, &exit_code);
		printf("GetExitCodeThread returns %x", exit_code);

		ret = true;
		break;
	}

	if (!ret)
	{
		if (lp_shellcode_remote) SIC_MARK;
		//TODO call VirtualFree(...)
	}

	if (h_remote_thread) CloseHandle(h_remote_thread);
	return ret;
}

//returns entry point in remote process
ULONG_PTR GetEnryPoint(const HANDLE hProcess)
{
	const auto h_process = hProcess;

	process_basic_information pbi;
	memset(&pbi, 0, sizeof(pbi));
	DWORD retlen = 0;

	auto status = ZwQueryInformationProcess(
		h_process,
		0,
		&pbi,
		sizeof(pbi),
		&retlen);

	const auto p_local_peb = REMOTE(peb, ULONG_PTR(pbi.peb_base_address));

	printf("\n");
	printf("from PEB: %p and %p\n", p_local_peb->reserved3[0], p_local_peb->reserved3[1]);

	const auto peb_remote_image_base = ULONG_PTR(p_local_peb->reserved3[1]); // TODO x64 POC only
	const auto p_remote_entry_point = FindEntryPoint2(h_process, peb_remote_image_base);
	return p_remote_entry_point;
}

//returns address of LoadLibraryA in remote process
ULONG_PTR GetRemoteLoadLibraryA(const HANDLE hProcess)
{
	const auto h_process = hProcess;
	DWORD n_modules;
	const auto ph_modules = GetRemoteModules(h_process, &n_modules);
	export_context my_context;
	my_context.module_name = "KERNEL32.dll";
	my_context.function_name = "LoadLibraryW";
	my_context.remote_function_address = 0;
	RemoteModuleWorker(h_process, ph_modules, n_modules, FindExport, &my_context);
	printf("kernel32!LoadLibraryA is at %llu \n", my_context.remote_function_address);

	return my_context.remote_function_address;
}

#if 0
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
#endif

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
#if 0
	auto res = Func();
	printf("%d", res);
	return 0;
#endif

	STARTUPINFO info = { sizeof(info) };
	PROCESS_INFORMATION process_info;
	::CreateProcess(L"C:\\Windows\\system32\\notepad.exe", nullptr,
		nullptr, nullptr, true, CREATE_SUSPENDED, nullptr, nullptr, &info, &process_info);

	Sleep(1000);
	
	//-------
	const auto h_process = process_info.hProcess;
	const auto p_remote_entry_point = GetEnryPoint(h_process);

	WORD orig_word;
	WORD pathed_word = 0xFEEB;
	ReadRemoteDataType<WORD>(h_process, p_remote_entry_point, &orig_word);
	WriteRemoteDataType<WORD>(h_process, p_remote_entry_point, &pathed_word);

	ResumeThread(process_info.hThread); //resumed pathed process
	Sleep(1000);

	const auto remote_load_library_address = GetRemoteLoadLibraryA(h_process);

#if 0
	entry_point_context my_context2;
	my_context2.remote_remote_entry_point = 0;
	RemoteModuleWorker(process_info.hProcess, ph_modules, n_modules, FindEntryPoint, &my_context2);
	printf("notepad entry pount is at %llu \n", my_context2.remote_remote_entry_point);
#endif

	InjectDll(h_process, L"E:\\Documents\\Visual Studio 2017\\Projects\\RE-S18\\x64\\Debug\\MyDll.dll", 
		PVOID(remote_load_library_address));
	Sleep(2000);

	auto status = ZwSuspendProcess(h_process);
	WriteRemoteDataType<WORD>(h_process, p_remote_entry_point, &orig_word);
	status = ZwResumeProcess(h_process);
	Sleep(10000);

	/*
	WaitForSingleObject(processInfo.hProcess, INFINITE);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
	*/
	return 0;
}

#endif

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
