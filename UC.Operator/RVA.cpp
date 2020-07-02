// UC.Cmd.RVA.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "RVA.h"
#include <stdio.h>
#include <tchar.h>
#include <regex>
#include <UC.Logger\UC.Logger.h>
#include <UC.Utils\UC.Utils.h>
#include <UC.Utils\UC.CommandFunctionType.h>

#include <psapi.h>
#pragma comment(lib,"psapi.lib")

#include "MsDiaHelper.h"
#include "EnumSymbols.h"

static HRESULT AppendRVA(const CString& modulePath, const CString& functionName, int ordinal, bool listUp, CString& output)
{
	UC_LOG(_T("Undercover"), _T("modulePath=[%s], functionName=[%s], ordinal=%d, listUp=%d"), modulePath, functionName, ordinal, listUp);

	// Make symbol file path from assembly path
	TCHAR symbolPath[MAX_PATH];
	ZeroMemory(symbolPath, sizeof(symbolPath));
	_tcscpy_s(symbolPath, (LPCTSTR)modulePath);
	PathRemoveExtension(symbolPath);
	_tcscat_s(symbolPath, _T(".pdb"));
	UC_LOG(_T("Undercover"), _T("symbolPath=[%s]"), symbolPath);

	// Function Symbols
	HRESULT hr = E_NOT_SET;
	DWORD RVA = 0;
	DWORD hitCount = 0;
	CMsDiaHelper msDiaHelper;
	msDiaHelper.Init();
	CEnumSymbols enumSymbols;
	enumSymbols.Init(msDiaHelper.GetFactory(), symbolPath, SymTagEnum::SymTagFunction, listUp ? nullptr : (LPCTSTR)functionName);
	enumSymbols.Walkaround([modulePath, functionName, ordinal, &output, listUp, &hitCount, &hr, &RVA](IDiaSymbol& symbol)
	{
		DWORD symTag = 0;
		HRESULT _hr = S_OK;
		_hr = symbol.get_symTag(&symTag);

		DWORD relativeVirtualAddress = 0;
		_hr = symbol.get_relativeVirtualAddress(&relativeVirtualAddress);
		if (relativeVirtualAddress == 0)
		{
			UC_LOG(_T("Undercover"), _T("return false, relativeVirtualAddress == 0"));
			return false;
		}

		CComBSTR bstrName;
		_hr = symbol.get_name(&bstrName);
		CComBSTR bstrUndecorated;
		_hr = symbol.get_undecoratedName(&bstrUndecorated);

		if (listUp && (functionName.IsEmpty() || 0 <= CString(bstrName).Find(functionName)))
		{
			output.AppendFormat(_T("0x%p\t% -30s\t%s\n"), (void*)relativeVirtualAddress, (LPCTSTR)bstrName, (LPCTSTR)bstrUndecorated);
			hr = S_OK;
			false;
		}

		// Print matched function only
		//UC_LOG(_T("Undercover"), _T("bstrName=[%s]"), CString(bstrName));
		if (functionName.Compare((LPCTSTR)bstrName) != 0)
		{
			return false;
		}

		if (hitCount < ordinal)
		{
			hitCount++;
			return false;
		}

		RVA = relativeVirtualAddress;
		hr = S_OK;
		return true;
	});

	if (listUp == false)
	{
		output.AppendFormat(_T("%s/%s"), modulePath, functionName);

		if (0 < ordinal)
		{
			output.AppendFormat(_T("@%d"), ordinal);
		}

		if (RVA)
		{
			output.AppendFormat(_T("/0x%p "), RVA);
		}
		else
		{
			output.Append(_T("(*** Not Found ***) "));
		}
	}

	return hr;
}

static bool ParseParameter(const CString& parameter, CString& moduleName, CString& functionName, int& ordinal, bool& listUp)
{
	std::wstring text = parameter;
	std::wsmatch matches;
	typedef struct
	{
		std::wregex regex;
		bool listUp;
		LPCTSTR pDescription;
	} Pattern;
	Pattern patterns[] = {
		{ std::wregex(LR"(^([^/?@\s]+)$)"), true, _T("{ModuleName}") },
		{ std::wregex(LR"(^([^/?@\s]+)\?$)"), true, _T("{ModuleName}?") },
		{ std::wregex(LR"(^([^/?@\s]+)\?([^/?@\s]+)$)"), true, _T("{ModuleName}?{PartOfFunctionName}") },
		{ std::wregex(LR"(^([^/?@\s]+)/([^/?@\s]+)$)"), false, _T("{ModuleName}/{FunctionName}") },
		{ std::wregex(LR"(^([^/?@\s]+)/([^/?@\s]+)@(\d+)$)"), false, _T("{ModuleName}/{FunctionName}@{Ordinal}") },
	};

	Pattern *pPattern = nullptr;
	for (size_t i = 0; i < _countof(patterns); i++)
	{
		matches.empty();
		if (std::regex_match(text, matches, patterns[i].regex))
		{
			pPattern = &patterns[i];
			break;
		}
	}

	//UC_LOG(_T("Undercover"), _T(">> --- matches ---"));
	//for (auto && match : matches)
	//{
	//	UC_LOG(_T("Undercover"), _T("%s"), match.str().c_str());
	//}
	//UC_LOG(_T("Undercover"), _T("<< --- matches ---"));

	if (pPattern == nullptr)
	{
		UC_LOG(_T("Undercover"), _T("Unmatched any patterns, text=[%s]"), parameter);
		return false;
	}

	UC_LOG(_T("Undercover"), _T("Matched pattern=[%s], text=[%s], matches.size()=%lu"), pPattern->pDescription, parameter, matches.size());

	// Initialize return values
	moduleName.Empty();
	functionName.Empty();
	ordinal = pPattern->listUp ? -1 : 0;
	listUp = pPattern->listUp;

	// Initialize
	if (matches.size() == 0)
	{
		_ftprintf_s(stderr, _T("Failed RVA parameter parsing [%s]\n"), parameter);
		UC_LOG(_T("Undercover"), _T("Failed parsing RVA parameter [%s]. matches.size() == 0"), parameter);
		return false;
	}

	if (2 <= matches.size())
	{
		moduleName = matches[1].str().c_str();
	}
	if (3 <= matches.size())
	{
		functionName = matches[2].str().c_str();
	}
	if (4 <= matches.size())
	{
		const CString& ordinalyString = matches[3].str().c_str();
		ULONGLONG ordinalAsULONGLONG = 0;
		if (UCUtils_ConvertToULONGLONG(ordinalyString, ordinalAsULONGLONG) == false)
		{
			_ftprintf_s(stderr, _T("Failed RVA parameter parsing [%s], Ordinal should be numeric.\n"), parameter);
			UC_LOG(_T("Undercover"), _T("Failed parsing RVA parameter [%s], Ordinal should be numeric."), parameter);
			return false;
		}
		ordinal = (int)ordinalAsULONGLONG;
	}

	UC_LOG(_T("Undercover"), _T("moduleName=[%s], functionName=[%s], ordinal=%d"), moduleName, functionName, ordinal);
	return true;
}

static HRESULT ParseRvaParameters(const std::vector<CString>& parameters, CString& output)
{
	for (size_t i = 0; i < parameters.size(); i++)
	{
		CString moduleName;
		CString functionName;
		int ordinal = -1;
		bool listUp = false;
		ParseParameter(parameters[i], moduleName, functionName, ordinal, listUp);
		if (1 < parameters.size() && listUp)
		{
			output.Append(_T("[ModuleName] or [ModuleName?FunctionName] can be used when 1 parameter only.\n"));
			return E_INVALIDARG;
		}
		AppendRVA(moduleName, functionName, ordinal, listUp, output);
	}
	output.Append(_T("\n"));
	return S_OK;
}

HRESULT RVA(const std::vector<CString>& parameters, CString& output)
{
	return UCUtils_RunCommand(parameters, output, ParseRvaParameters);
}
