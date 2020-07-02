#include "Process.h"
//#include <atlstr.h>
#include <winternl.h>
//#include <libloaderapi.h>
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

HRESULT GetProcessInfoListFromProcessNameOrId(LPCTSTR pInputString, std::vector<UCProcessInfo>& processInfoList)
{
	// Get Process ID if pInputString is number.
	TCHAR targetProcessName[MAX_PATH];
	ZeroMemory(targetProcessName, sizeof(targetProcessName));
	ULONGLONG targetProcessId = 0;
	if (UCUtils_ConvertToULONGLONG(pInputString, targetProcessId))
	{
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)targetProcessId);
		GetModuleBaseName(hProcess, nullptr, targetProcessName, sizeof(targetProcessName) / sizeof(TCHAR));
		CloseHandle(hProcess);
	}
	else
	{
		_tcscpy_s(targetProcessName, pInputString);
	}

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
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIds[i]);
		if (hProcess == NULL)
		{
			continue;
		}

		TCHAR processName[MAX_PATH];
		ZeroMemory(processName, sizeof(processName));
		GetModuleBaseName(hProcess, nullptr, processName, sizeof(processName) / sizeof(TCHAR));

		if ((0 < targetProcessId && targetProcessId == processIds[i]) || _tcsicmp(processName, targetProcessName) == 0)
		{
			CString commandLine;
			GetRemoteCommandLine(hProcess, commandLine);
			UCProcessInfo processInfo = { 0 };
			processInfo.id = processIds[i];
			_tcscat_s(processInfo.name, targetProcessName);
			_tcscat_s(processInfo.commandLine, (LPCTSTR)commandLine);

			processInfoList.push_back(processInfo);
		}

		CloseHandle(hProcess);
	}
	return (processInfoList.size() == 0) ? E_INVALIDARG : S_OK;
}
