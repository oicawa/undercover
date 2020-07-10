// UC.Cmd.Inject.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Module.h"
#include "Process.h"
#include <UC.Logger\UC.Logger.h>
#include <UC.Utils\UC.Utils.h>
#include <map>
#include "Privilege.h"

static HMODULE GetRemoteProcessModuleHandle(HANDLE hProcess, const CString& dllPath)
{
	TCHAR fullPath[MAX_PATH];
	ZeroMemory(fullPath, sizeof(fullPath));
	_tcscpy_s(fullPath, dllPath);
	GetFullPathName(dllPath, MAX_PATH - 1, fullPath, nullptr);
	UC_LOG(_T("Undercover"), _T("dllPath=[%s] -> [%s]"), dllPath, fullPath);

	std::map<CString, UCModuleInfo> moduleInfoMap;
	UCUtils_GetModuleInfoList(hProcess, moduleInfoMap);
	UC_LOG(_T("Undercover"), _T("moduleInfoMap.size()=%Iu"), moduleInfoMap.size());
	CString key(fullPath);
	auto& itModuleInfo = moduleInfoMap.find(key.MakeUpper());
	if (itModuleInfo == moduleInfoMap.end())
	{
		UC_LOG(_T("Undercover"), _T("No module which has full path [%s]"), fullPath);
		return nullptr;
	}

	return itModuleInfo->second.handle;
}

static HRESULT AttachDll(HANDLE hProcess, DWORD processId, const CString& dllPath, CString& output)
{
	HRESULT result = S_OK;
	HMODULE hModule = nullptr;
	HANDLE hThread = nullptr;
	COPYDATASTRUCT copyData = { 0 };

	char dllPathA[MAX_PATH];
	ZeroMemory(dllPathA, sizeof(dllPathA));
	size_t count = 0;
	wcstombs_s(&count, dllPathA, dllPath, _countof(dllPathA));

	SIZE_T size = sizeof(char) * strlen(dllPathA) + 1;
	void *pDataMemory = VirtualAllocEx(hProcess, NULL, size, MEM_COMMIT, PAGE_READWRITE);
	if (pDataMemory == NULL)
	{
		UC_LOG(_T("Undercover"), _T("VirtualAllocEx failed. (ProcessID=%lu)"), processId);
		result = E_POINTER;
		goto RELEASE;
	}

	if (WriteProcessMemory(hProcess, pDataMemory, (void *)dllPathA, size, NULL) == FALSE)
	{
		UC_LOG(_T("Undercover"), _T("WriteProcessMemory failed. (ProcessID=%lu)"), processId);
		result = E_ACCESSDENIED;
		goto RELEASE;
	}

	HMODULE hKernel32 = GetModuleHandle(_T("kernel32"));
	FARPROC loadLibrary = GetProcAddress(hKernel32, "LoadLibraryA");
	hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibrary, pDataMemory, 0, NULL);
	if (hThread == NULL)
	{
		// 32 bit (WOW64) -> 64 bit (Native) won't work
		TCHAR errmsg[BUFSIZ];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, (LPWSTR)errmsg, sizeof(errmsg), NULL);
		UC_LOG(_T("Undercover"), _T("CreateRemoteThread failed. %s"), errmsg);
		result = E_FAIL;
		goto RELEASE;
	}

	WaitForSingleObject(hThread, INFINITE);

	UC_LOG(_T("Undercover"), _T("Dll is loaded."));

RELEASE:
	if (pDataMemory)
	{
		VirtualFreeEx(hProcess, pDataMemory, 0, MEM_RELEASE);
		pDataMemory = nullptr;
	}
	if (hThread)
	{
		CloseHandle(hThread);
		hThread = NULL;
	}
	return result;
}

static HRESULT Attach(const std::vector<CString>& parameters, CString& output)
{
	if (parameters.size() < 2)
	{
		output.AppendFormat(_T("'Module Attache' requires at least more 2 arguments ([ProcessName or ProcessID] [DllPath [DllPath [...]]]\n"));
		return E_INVALIDARG;
	}

	CPrivilege privilege;
	privilege.Init();

	HRESULT hr = S_OK;

	std::vector<UCProcessInfo> processInfoList;
	hr = GetProcessInfoList(parameters[0], processInfoList);
	if (FAILED(hr))
	{
		if (hr == E_FAIL)
		{
			output.AppendFormat(_T("Parameter [0](=%s) was target process name. But failed to get all process information.\n"), parameters[0]);
		}
		else if (hr == E_INVALIDARG)
		{
			output.AppendFormat(_T("Parameter [0](=%s) was target process name. But not found the specified process name.\n"), parameters[0]);
		}
		else
		{
			output.AppendFormat(_T("Unexpected fail. Parameter [0](=%s)\n"), parameters[0]);
		}
		return E_FAIL;
	}

	for (auto& processInfo : processInfoList)
	{
		// Open Process Handler
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processInfo.id);
		if (hProcess == NULL)
		{
			output.AppendFormat(_T("Open process failed. (pid=%lu)"), processInfo.id);
			continue;
		}

		// Load Libraries

		for (size_t i = 1; i < parameters.size(); i++)
		{
			hr = AttachDll(hProcess, processInfo.id, parameters[i], output);
			if (FAILED(hr))
			{
				output.AppendFormat(_T("Attach dll failed. (pid=%lu, dll=[%s])"), processInfo.id, parameters[i]);
				break;
			}
		}

		if (hProcess)
		{
			CloseHandle(hProcess);
			hProcess = NULL;
		}
	}

	return hr;
}

static HRESULT DetachDll(HANDLE hProcess, DWORD processId, const CString& dllPath)
{
	HANDLE hThread = nullptr;
	HMODULE hModule = nullptr;

	hModule = GetRemoteProcessModuleHandle(hProcess, dllPath);
	if (hModule == nullptr)
	{
		UC_LOG(_T("Undercover"), _T("ProcessID=%d, Module=[%s], Handle=0x%p"), processId, dllPath, hModule);
		return E_HANDLE;
	}

	HRESULT hr = S_OK;
	SIZE_T size = sizeof(HMODULE);
	void *pDataMemory = VirtualAllocEx(hProcess, NULL, size, MEM_COMMIT, PAGE_READWRITE);
	if (pDataMemory == NULL)
	{
		UC_LOG(_T("Undercover"), _T("VirtualAllocEx failed. (ProcessID=%lu)"), processId);
		hr = E_POINTER;
		goto RELEASE;
	}

	HMODULE hKernel32 = GetModuleHandle(_T("kernel32"));
	FARPROC freeLibrary = GetProcAddress(hKernel32, "FreeLibrary");
	if (freeLibrary == nullptr)
	{
		UC_LOG(_T("Undercover"), _T("GetProcAddress failed. (ProcessID=%lu)"), processId);
		hr = E_POINTER;
		goto RELEASE;
	}

	hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)freeLibrary, (void *)hModule, 0, NULL);
	if (hThread == NULL)
	{
		// 32 bit (WOW64) -> 64 bit (Native) won't work
		TCHAR errmsg[BUFSIZ];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, (LPWSTR)errmsg, sizeof(errmsg), NULL);
		UC_LOG(_T("Undercover"), _T("CreateRemoteThread failed. %s"), errmsg);
		return E_HANDLE;
		goto RELEASE;
	}

	WaitForSingleObject(hThread, INFINITE);

RELEASE:
	if (pDataMemory)
	{
		VirtualFreeEx(hProcess, pDataMemory, 0, MEM_RELEASE);
		pDataMemory = nullptr;
	}
	if (hThread)
	{
		CloseHandle(hThread);
		hThread = NULL;
	}
	return hr;
}

static HRESULT Detach(const std::vector<CString>& parameters, CString& output)
{
	bool result = false;
	HMODULE hModule = nullptr;
	HANDLE hThread = nullptr;
	COPYDATASTRUCT copyData = { 0 };

	CPrivilege privilege;
	privilege.Init();

	std::vector<UCProcessInfo> processInfoList;
	HRESULT hr = GetProcessInfoList(parameters[0], processInfoList);
	if (FAILED(hr))
	{
		if (hr == E_FAIL)
		{
			output.AppendFormat(_T("[NG] Parameter [1](=%s) was target process name. But failed to get all process information.\n"), parameters[1]);
		}
		else if (hr == E_INVALIDARG)
		{
			output.AppendFormat(_T("[NG] Parameter [1](=%s) was target process name. But not found the specified process name.\n"), parameters[1]);
		}
		else
		{
			output.AppendFormat(_T("[NG] Unexpected fail. Parameter [1](=%s)\n"), parameters[1]);
		}
		return hr;
	}

	for (auto& processInfo : processInfoList)
	{
		// Open Process Handler
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processInfo.id);
		if (hProcess == NULL)
		{
			output.AppendFormat(_T("OpenProcess failed. (ProcessID=%lu)"), processInfo.id);
			continue;
		}

		// Detach Libraries
		for (size_t i = 1; i < parameters.size(); i++)
		{
			DetachDll(hProcess, processInfo.id, parameters[i]);
		}

		if (hProcess)
		{
			CloseHandle(hProcess);
			hProcess = NULL;
		}
	}

	return result;
}

HRESULT Module(const std::vector<CString>& parameters, CString& output)
{
	if (parameters.size() < 1)
	{
		output.Append(_T("[Module] command requires sub command 'Attach' or 'Detach'.\n"));
		return E_INVALIDARG;
	}

	auto itParameter = parameters.begin();
	CString mode = *itParameter;
	itParameter++;
	std::vector<CString> localParameters(itParameter, parameters.end());

	UCCommandFunction pUCCommandFunction = nullptr;
	if (mode.Compare(_T("Attach")) == 0)
	{
		pUCCommandFunction = Attach;
	}
	else if (mode.Compare(_T("Detach")) == 0)
	{
		pUCCommandFunction = Detach;
	}
	else
	{
		output.AppendFormat(_T("Mode name [%s] is invalid.\n"), mode);
		return E_INVALIDARG;
	}

	return UCUtils_RunCommand(localParameters, output, [pUCCommandFunction](const std::vector<CString>& parameters, CString& output)
	{
		return pUCCommandFunction(parameters, output);
	});
}
