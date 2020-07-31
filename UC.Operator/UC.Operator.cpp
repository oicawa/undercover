// UC.Operator.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <atlbase.h>
#include <atlstr.h>
#include <atlconv.h>
#include <windows.h>
//#include <winnt.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <vector>
#include <map>

#include <UC.Logger\UC.Logger.h>
#include <UC.Utils\UC.Utils.h>
#include <UC.Utils\UC.CommandFunctionType.h>
#include <UC.Utils\TextFile.h>

#include "RVA.h"
#include "Module.h"
#include "Order.h"

void GetTargetModulePath(const CString& command, CString& moduleName, CString& modulePath)
{
	moduleName.Format(L"UC.Cmd.%s.dll", command);

	UCUtils_GetBaseDirectory(modulePath);
	modulePath.AppendFormat(_T("\\UC.Commands\\%s"), moduleName);
}

void DisplayCommandList()
{
	UC_LOG(_T("Undercover"), _T("Display command list."));

	CTextFile textFile;
	textFile.Open(L".\\UC.Operator.txt");
	CString line;
	while (textFile.ReadLine(line))
	{
		_tprintf(L"%s\n", line);
	}
}

std::map<CString, UCCommandFunction> g_functions;

int main(int argc, const char** argv)
{
	g_functions[_T("RVA")] = RVA;
	g_functions[_T("Module")] = Module;
	g_functions[_T("Order")] = Order;

	if (argc < 2)
	{
		DisplayCommandList();
		return 0;
	}

	std::vector<CString> parameters;
	UC_LOG(_T("Undercover"), _T("argc=%d"), argc);
	for (int i = 2; i < argc; i++)
	{
		CString argument(argv[i]);
		UC_LOG(_T("Undercover"), _T("argv[%d]=[%s]"), i, argument);
		parameters.push_back(argument);
	}

	CString command(argv[1]);
	auto it = g_functions.find(command);
	if (it == g_functions.end())
	{
		CString log;
		log.Format(_T("Command [%s] is not found."), (LPCTSTR)command);
		_ftprintf(stderr, log);
		UC_LOG(_T("Undercover"), _T("[NG] %s"), log);
		return -1;
	}
	UCCommandFunction pUCCommandFunction = it->second;
	
	CString output;
	HRESULT hr = pUCCommandFunction(parameters, output);
	if (FAILED(hr))
	{
		UC_LOG(_T("Undercover"), _T("[NG] %s"), output);
		UC_LOG(_T("Undercover"), _T("[NG] Command [%s] is failed. (hr=0x%x)"), (LPCTSTR)command, hr);
		_ftprintf(stderr, output);
	}
	else
	{
		_ftprintf(stdout, output);
	}

	//TCHAR buffer[BUFSIZ];
	//_fgetts(buffer, BUFSIZ - 1, stdin);
	return FAILED(hr) ? -1 : 0;
}
