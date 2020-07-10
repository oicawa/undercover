// UC.Cmd.Order.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Order.h"
#include "Process.h"
#include <UC.Logger\UC.Logger.h>
#include <UC.Utils\UC.Utils.h>
#include <UC.Utils\Pipe.h>
#include "Privilege.h"

static HRESULT ParseOrderParameters(const std::vector<CString>& parameters, CString& output)
{
	if (parameters.size() < 3)
	{
		UC_LOG(_T("Undercover"), _T("Order function requires ProcessID, Module Name, and Function Name arguments."));
		return E_INVALIDARG;
	}

	CPrivilege privilege;
	privilege.Init();

	std::vector<UCProcessInfo> processInfoList;
	HRESULT hr = GetProcessInfoList(parameters[0], processInfoList);
	if (FAILED(hr))
	{
		output.Format(_T("Parameter [0](=%s) is not a existing process ID nor process name.\n"), parameters[0]);
		UC_LOG(_T("Undercover"), _T("Parameter [0](=%s) is not a positive number. Specify Process ID."), parameters[0]);
		return hr;
	}

	for (auto& processInfo : processInfoList)
	{
		CString inputString;
		const TCHAR SEPARATOR = _T('\n');
		inputString.AppendChar(SEPARATOR);
		for (size_t i = 1; i < parameters.size(); i++)
		{
			inputString.AppendFormat(_T("%s%c"), parameters[i], SEPARATOR);
		}
		inputString.TrimRight();
		size_t byteSize = sizeof(TCHAR) * (inputString.GetLength() + 1);
		std::vector<BYTE> input(byteSize);
		memcpy(input.data(), (BYTE*)inputString.GetBuffer(), byteSize);

		CPipe pipe;
		pipe.Create(processInfo.id, false);
		pipe.Write(input);
		std::vector<BYTE> outputBytes;
		pipe.Read(outputBytes);

		output.AppendFormat(_T("%s\n"), (LPTSTR)outputBytes.data());
	}

	return S_OK;
}

HRESULT Order(const std::vector<CString>& parameters, CString& output)
{
	return UCUtils_RunCommand(parameters, output, ParseOrderParameters);
}