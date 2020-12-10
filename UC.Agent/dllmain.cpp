// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <atlstr.h>
#include <vector>
#include <UC.Logger\UC.Logger.h>
#include <UC.Utils\UC.CommandFunctionType.h>
#include <UC.Utils\Pipe.h>

static CRITICAL_SECTION g_cs = { 0 };
static HANDLE g_hNamedPipeThread = NULL;

void DoCommand(const CString& moduleName, const CString functionName, const std::vector<CString>& parameters, CString& output)
{
	// Logging.
	UC_LOG(_T("Undercover"), _T("Module=[%s],Function=[%s]"), (LPCTSTR)moduleName, (LPCTSTR)functionName);
	for (size_t i = 0; i < parameters.size(); i++)
	{
		UC_LOG(_T("Undercover"), _T("    %s"), (LPCTSTR)parameters[i]);
	}

	// Module Handle
	HMODULE hModule = GetModuleHandle(moduleName);
	UC_LOG(_T("Undercover"), _T("hModule    = 0x%p"), hModule);
	if (hModule == NULL)
	{
		return;
	}

	// Function Pointer
	char functionNameA[BUFSIZ];
	ZeroMemory(functionNameA, sizeof(functionNameA));
	strcpy_s(functionNameA, CW2A(functionName));
	FARPROC pFunction = GetProcAddress(hModule, functionNameA);
	UC_LOG(_T("Undercover"), _T("pFunction  = 0x%p"), pFunction);
	if (pFunction == NULL)
	{
		return;
	}

	UCCommandFunctionPointer pUCCommandFunction = (UCCommandFunctionPointer)pFunction;

	pUCCommandFunction(parameters, output);
}

void NamedPipeThread()
{
	CPipe pipe;
	pipe.Create(GetCurrentProcessId(), true);
	pipe.Run([](std::vector<byte>& input, std::vector<byte>& output)
	{
		size_t sizeOfInputWithNullTerminate = input.size() + sizeof(TCHAR);
		TCHAR* pInput = (TCHAR*)malloc(sizeOfInputWithNullTerminate);
		if (pInput == NULL)
		{
			UC_LOG(_T("Undercover"), _T("[NG] pInput == NULL, malloc failed"));
			return;
		}
		ZeroMemory(pInput, sizeof(sizeOfInputWithNullTerminate));

		memcpy_s(pInput, sizeOfInputWithNullTerminate, input.data(), input.size());
		TCHAR separator[] = { pInput[0], _T('\0') };
		CString inputString = &pInput[1];
		free(pInput);

		CString moduleName;
		CString functionName;
		std::vector<CString> parameters;
		int cursor = 0;
		while (true)
		{
			CString row = inputString.Tokenize(separator, cursor);
			if (row.IsEmpty())
			{
				break;
			}
			if (moduleName.IsEmpty())
			{
				moduleName = row;
				continue;
			}
			if (functionName.IsEmpty())
			{
				functionName = row;
				continue;
			}
			parameters.push_back(row);
		}

		CString outputString;
		DoCommand(moduleName, functionName, parameters, outputString);

		size_t sizeOfOutput = outputString.GetLength() * sizeof(TCHAR);
		byte* pOutput = (byte*)(LPCTSTR)outputString;
		output.reserve(sizeOfOutput);
		for (size_t i = 0; i < sizeOfOutput; i++)
		{
			output.push_back(pOutput[i]);
		}
		for (size_t i = 0; i < sizeof(TCHAR); i++)
		{
			output.push_back(0x0);
		}
	});

	return;
}

void CreateNamedPipeThread(HANDLE& hNamedPipeThread)
{
	DWORD threadId = 0;
	hNamedPipeThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)NamedPipeThread, NULL, 0, &threadId);
	UC_LOG(_T("Undercover"), _T("Call CreateThread(...), hNamedPipeThread==0x%p"), hNamedPipeThread);
	if (hNamedPipeThread == NULL)
	{
		// 32 bit (WOW64) -> 64 bit (Native) won't work
		return;
	}
}

void DeleteNamedPipeThread(CRITICAL_SECTION& cs, HANDLE& hNamedPipeThread)
{
	EnterCriticalSection(&cs);

	if (hNamedPipeThread)
	{
		CloseHandle(hNamedPipeThread);
		hNamedPipeThread = NULL;
		UC_LOG(_T("Undercover"), _T("CloseHandle(hNamedPipeThread)"));
	}

	LeaveCriticalSection(&cs);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		InitializeCriticalSection(&g_cs);
		CreateNamedPipeThread(g_hNamedPipeThread);
		UC_LOG(_T("Undercover"), _T("Call CreateNamepdPipeThread(...), g_hNamedPipeThread=0x%p"), g_hNamedPipeThread);
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		DeleteNamedPipeThread(g_cs, g_hNamedPipeThread);
		UC_LOG(_T("Undercover"), _T("Call DeleteNamepdPipeThread(...), g_hNamedPipeThread=0x%p"), g_hNamedPipeThread);
		DeleteCriticalSection(&g_cs);
		break;
	}
	return TRUE;
}

