// Commands.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <vector>
#include <atlstr.h>
#include "..\UC.Logger\UC.Logger.h"

HRESULT HogeHoge(const std::vector<CString>& parameters, CString& output)
{
	return S_OK;
}

HRESULT RemoveModule(const std::vector<CString>& parameters, CString& output)
{
	if (parameters.size() < 1)
	{
		output.Format(_T("No Modules."));
		DEBUG_LOG(_T("Undercover"), output);
		return E_INVALIDARG;
	}

	DWORD countOK = 0;
	DWORD countNG = 0;
	for (size_t i = 0; i < parameters.size(); i++)
	{
		CString log;
		const CString& moduleName = parameters[i];
		log.Format(_T("%lu: ModuleName=[%s]"), i, moduleName);

		HMODULE hModule = GetModuleHandle(moduleName);
		log.AppendFormat(_T(", Handle=0x%p"), hModule);
		if (hModule == nullptr)
		{
			log.Insert(0, _T("[NG] "));
			DEBUG_LOG(_T("Undercover"), log);
			output.AppendFormat(_T("%s\n"), log);
			countNG++;
			continue;
		}

		BOOL result = FreeLibrary(hModule);
		log.AppendFormat(_T(", FreeLibrary"));
		log.Insert(0, result ? _T("[OK] ") : _T("[NG] "));
		DEBUG_LOG(_T("Undercover"), log);
		output.AppendFormat(_T("%s\n"), log);
		DWORD& count = result ? countOK : countNG;
		count++;
	}

	return (countNG == 0) ? S_OK : ((countOK == 0) ? E_FAIL : S_FALSE);
}
