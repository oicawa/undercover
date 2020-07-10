#include "Process.h"
#include <winternl.h>
#include <psapi.h>
#pragma comment(lib,"psapi.lib")

#include <UC.Logger\UC.Logger.h>
#include <UC.Utils\UC.Utils.h>

static DWORD GetRemoteCommandLine(HANDLE hProcess, CString& commandLine)
{
	struct RTL_USER_PROCESS_PARAMETERS_I
	{
		BYTE Reserved1[16];
		PVOID Reserved2[10];
		UNICODE_STRING ImagePathName;
		UNICODE_STRING CommandLine;
	};

	struct PEB_INTERNAL
	{
		BYTE Reserved1[2];
		BYTE BeingDebugged;
		BYTE Reserved2[1];
		PVOID Reserved3[2];
		struct PEB_LDR_DATA* Ldr;
		RTL_USER_PROCESS_PARAMETERS_I* ProcessParameters;
		BYTE Reserved4[104];
		PVOID Reserved5[52];
		struct PS_POST_PROCESS_INIT_ROUTINE* PostProcessInitRoutine;
		BYTE Reserved6[128];
		PVOID Reserved7[1];
		ULONG SessionId;
	};

	typedef NTSTATUS(NTAPI* NtQueryInformationProcessPtr)(
		IN HANDLE ProcessHandle,
		IN PROCESSINFOCLASS ProcessInformationClass,
		OUT PVOID ProcessInformation,
		IN ULONG ProcessInformationLength,
		OUT PULONG ReturnLength OPTIONAL);

	typedef ULONG(NTAPI* RtlNtStatusToDosErrorPtr)(NTSTATUS Status);

	// Locating functions
	HINSTANCE hNtDll = GetModuleHandleW(L"ntdll.dll");
	NtQueryInformationProcessPtr NtQueryInformationProcess = (NtQueryInformationProcessPtr)GetProcAddress(hNtDll, "NtQueryInformationProcess");
	RtlNtStatusToDosErrorPtr RtlNtStatusToDosError = (RtlNtStatusToDosErrorPtr)GetProcAddress(hNtDll, "RtlNtStatusToDosError");

	if (!NtQueryInformationProcess || !RtlNtStatusToDosError)
	{
		UC_LOG(_T("Undercover"), _T("[NG] Functions cannot be located. NtQueryInformationProcess=0x%p, RtlNtStatusToDosError=0x%p"), NtQueryInformationProcess, RtlNtStatusToDosError);
		return 0;
	}

	// Get PROCESS_BASIC_INFORMATION
	PROCESS_BASIC_INFORMATION pbi;
	ULONG len;
	NTSTATUS status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &len);
	SetLastError(RtlNtStatusToDosError(status));
	if (NT_ERROR(status) || !pbi.PebBaseAddress)
	{
		UC_LOG(_T("Undercover"), _T("[NG] NtQueryInformationProcess(ProcessBasicInformation) failed."));
		return 0;
	}

	// Read PEB memory block
	SIZE_T bytesRead = 0;
	PEB_INTERNAL peb;
	if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb, sizeof(peb), &bytesRead))
	{
		UC_LOG(_T("Undercover"), _T("[NG] Reading PEB failed."));
		return 0;
	}

	// Obtain size of commandline string
	RTL_USER_PROCESS_PARAMETERS_I upp;
	if (!ReadProcessMemory(hProcess, peb.ProcessParameters, &upp, sizeof(upp), &bytesRead))
	{
		UC_LOG(_T("Undercover"), _T("[NG] Reading USER_PROCESS_PARAMETERS failed."));
		return 0;
	}

	if (!upp.CommandLine.Length)
	{
		UC_LOG(_T("Undercover"), _T("[NG] Command line length is 0."));
		return 0;
	}

	wchar_t buffer[BUFSIZ + 1];
	ZeroMemory(buffer, sizeof(buffer));
	size_t bufferCount = (sizeof(buffer) / sizeof(wchar_t)) - 1;

	// Check the buffer size
	DWORD dwNeedLength = (upp.CommandLine.Length + 1) / sizeof(wchar_t) + 1;
	if (bufferCount < dwNeedLength)
	{
		UC_LOG(_T("Undercover"), _T("[NG] Not enough buffer."));
		return dwNeedLength;
	}

	// Get the actual command line
	if (!ReadProcessMemory(hProcess, upp.CommandLine.Buffer, buffer, upp.CommandLine.Length, &bytesRead))
	{
		UC_LOG(_T("Undercover"), _T("[NG] Reading command line failed."));
		return 0;
	}

	commandLine = buffer;

	return bytesRead / sizeof(wchar_t);
}

void AssgineUCProcessInfo(const HANDLE hProcess, const DWORD processId, LPCTSTR pProcessName, UCProcessInfo& processInfo)
{
	CString commandLine;
	GetRemoteCommandLine(hProcess, commandLine);

	processInfo.id = processId;
	_tcscat_s(processInfo.name, pProcessName);
	_tcscat_s(processInfo.commandLine, (LPCTSTR)commandLine);
}

HRESULT GetProcessInfoListFromProcessId(const DWORD processId, std::vector<UCProcessInfo>& processInfoList)
{
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
	if (hProcess == NULL)
	{
		UC_LOG(L"Undercover", L"processId=%lu", processId);
		return E_FAIL;
	}

	TCHAR targetProcessName[MAX_PATH];
	ZeroMemory(targetProcessName, sizeof(targetProcessName));
	GetModuleBaseName(hProcess, nullptr, targetProcessName, sizeof(targetProcessName) / sizeof(TCHAR));
	CloseHandle(hProcess);

	UCProcessInfo processInfo = { 0 };
	AssgineUCProcessInfo(hProcess, processId, targetProcessName, processInfo);
	processInfoList.push_back(processInfo);
}

HRESULT GetProcessInfoListFromProcessName(LPCTSTR pProcessName, std::vector<UCProcessInfo>& processInfoList)
{
	DWORD processIds[1024];
	ZeroMemory(processIds, sizeof(processIds));
	DWORD neededSizeOfProcessIds = 0;
	if (EnumProcesses(processIds, sizeof(processIds), &neededSizeOfProcessIds) == FALSE)
	{
		UC_LOG(_T("Undercover"), _T("[NG] Failed EnumProcesses."));
		return E_FAIL;
	}

	int countOfProcessIds = neededSizeOfProcessIds / sizeof(DWORD);
	for (int i = 0; i < countOfProcessIds; i++)
	{
		//HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIds[i]);
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processIds[i]);
		if (hProcess == NULL)
		{
			UC_LOG(L"Undercover", L"processId[%d]=%lu", i, processIds[i]);
			continue;
		}

		TCHAR processName[MAX_PATH];
		ZeroMemory(processName, sizeof(processName));
		GetModuleBaseName(hProcess, nullptr, processName, sizeof(processName) / sizeof(TCHAR));
		UC_LOG(L"Undercover", L"processId[%d]=%lu, processName=[%s]", i, processIds[i], processName);

		if (_tcsicmp(processName, pProcessName) == 0)
		{
			UCProcessInfo processInfo = { 0 };
			AssgineUCProcessInfo(hProcess, processIds[i], pProcessName, processInfo);
			processInfoList.push_back(processInfo);
		}

		CloseHandle(hProcess);
	}

	return (processInfoList.size() == 0) ? E_INVALIDARG : S_OK;
}

HRESULT GetProcessInfoList(LPCTSTR pInputString, std::vector<UCProcessInfo>& processInfoList)
{
	// Get Process ID if pInputString is number.
	ULONGLONG targetProcessId = 0;
	HRESULT hr = S_OK;
	if (UCUtils_ConvertToULONGLONG(pInputString, targetProcessId))
	{
		hr = GetProcessInfoListFromProcessId(targetProcessId, processInfoList);
	}
	else
	{
		hr = GetProcessInfoListFromProcessName(pInputString, processInfoList);
	}

	return hr;
}
