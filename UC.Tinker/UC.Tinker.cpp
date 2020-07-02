// UC.Tinker.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "UC.Tinker.h"
#include <UC.Logger\UC.Logger.h>
#include <UC.Utils\UC.Utils.h>
#include <3rdParty\includes\detours.h>
#include <map>
#include <algorithm>
#include <iterator>
#include <UC.Utils\TextFile.h>

typedef struct
{
	TCHAR line[BUFSIZ];
	void* pOriginal;
	void* pReplacing;
	void* pTrampoline;
} ReplacingEntry;

static std::map<void*, ReplacingEntry> g_replacings;	// Key:New Address


HRESULT UCTinker_List(const std::vector<CString>& parameters, CString& output)
{
	UC_LOG(_T("Undercover"), _T("Called List function."));

	output.Format(_T("%-18s %-18s %-18s %s\n"), _T("Replaced Address"), _T("Orignal Address"), _T("Trampoline"), _T("line"));
	for (auto it = g_replacings.begin(); it != g_replacings.end(); it++)
	{
		ReplacingEntry& entry = it->second;
		output.AppendFormat(_T("0x%p 0x%p 0x%p [%s]\n"), entry.pReplacing, entry.pOriginal, entry.pTrampoline, entry.line);
	}
	return S_OK;
}

static HRESULT ParseLine(const CString& line, CString& orgModuleName, ULONGLONG& orgRVA, CString& newModuleName, ULONGLONG& newRVA)
{
	std::vector<CString> parameters;
	HRESULT hr = UCUtils_Tokenize(line, _T(' '), parameters);
	if (FAILED(hr))
	{
		return hr;
	}
	if (parameters.size() != 2)
	{
		return E_INVALIDARG;
	}
	CString orgPart = parameters[0];;
	CString newPart = parameters[1];;
	UC_LOG(_T("Undercover"), _T("orgPart=[%s], newPart=[%s]"), orgPart, newPart);

	// Original
	parameters.clear();
	hr = UCUtils_Tokenize(orgPart, _T('/'), parameters);
	if (FAILED(hr))
	{
		return hr;
	}
	if (parameters.size() != 3)
	{
		return E_INVALIDARG;
	}
	orgModuleName = parameters[0];
	CString orgRVAString = parameters[2];
	if (UCUtils_ConvertToULONGLONG(orgRVAString, orgRVA) == false)
	{
		return E_INVALIDARG;
	}

	// New
	parameters.clear();
	hr = UCUtils_Tokenize(newPart, _T('/'), parameters);
	if (FAILED(hr))
	{
		return hr;
	}
	if (parameters.size() != 3)
	{
		return E_INVALIDARG;
	}
	newModuleName = parameters[0];
	CString newRVAString = parameters[2];
	if (UCUtils_ConvertToULONGLONG(newRVAString, newRVA) == false)
	{
		return E_INVALIDARG;
	}

	return S_OK;
}

HRESULT UCTinker_Replace(const std::vector<CString>& parameters, CString& output)
{
	// UC.Operator.exe Order [Process ID/Name] UC.Tinker.dll UCTinker_Replace [Replacing list file path]
	UC_LOG(_T("Undercover"), _T("Called Replace function."));

	if (parameters.size() != 1)
	{
		output = _T("[NG] UCTinker_Replace requires 2 argument as replacing list file path.\n");
		UC_LOG(_T("Undercover"), output);
		return E_INVALIDARG;
	}

	std::map<CString, UCModuleInfo> moduleInfoMap;
	UCUtils_GetModuleInfoList(GetCurrentProcess(), moduleInfoMap);

	DetourRestoreAfterWith();

	DetourTransactionBegin();

	DetourUpdateThread(GetCurrentThread());

	CTextFile file;
	file.Open(parameters[0]);
	CString line;
	while (file.ReadLine(line))
	{
		CString orgModuleName;
		ULONGLONG orgRVA = 0x0;
		CString newModuleName;
		ULONGLONG newRVA = 0x0;
		HRESULT hr = ParseLine(line, orgModuleName, orgRVA, newModuleName, newRVA);
		UC_LOG(_T("Undercover"), _T("line=[%s], orgModuleName=[%s], orgRVA=0x%p, newModuleName=[%s], newRVA=0x%p"), line, orgModuleName, orgRVA, newModuleName, newRVA);
		if (FAILED(hr))
		{
			// Error Output Message
			output.AppendFormat(_T("[NG] line=[%s]\n"), line);
			continue;
		}
		TCHAR orgModuleFullPath[MAX_PATH];
		ZeroMemory(orgModuleFullPath, sizeof(orgModuleFullPath));
		GetFullPathName(orgModuleName, MAX_PATH - 1, orgModuleFullPath, nullptr);
		auto& itOrgModuleInfo = moduleInfoMap.find(orgModuleFullPath);
		if (itOrgModuleInfo == moduleInfoMap.end())
		{
			// Not target entry.
			output.AppendFormat(_T("[--] line=[%s], Original Module [%s] is not target.\n"), line, orgModuleFullPath);
			continue;
		}

		TCHAR newModuleFullPath[MAX_PATH];
		ZeroMemory(newModuleFullPath, sizeof(newModuleFullPath));
		GetFullPathName(newModuleName, MAX_PATH - 1, newModuleFullPath, nullptr);
		auto& itNewModuleInfo = moduleInfoMap.find(newModuleFullPath);
		if (itNewModuleInfo == moduleInfoMap.end())
		{
			// This module should be loaded.
			output.AppendFormat(_T("[--] line=[%s], Replacing Module [%s] hasn't been loaded.\n"), line, newModuleFullPath);
			continue;
		}

		PVOID pOrgAddress = (PVOID)((ULONGLONG)itOrgModuleInfo->second.baseAddress + orgRVA);
		PVOID pNewAddress = (PVOID)((ULONGLONG)itNewModuleInfo->second.baseAddress + newRVA);

		auto& itEntry = g_replacings.find(pNewAddress);
		if (itEntry != g_replacings.end())
		{
			// This function already been replaced.
			output.AppendFormat(_T("[--] line=[%s], Replacing Function [0x%p] has already been replaced.\n"), line, pNewAddress);
			continue;
		}

		ReplacingEntry emptyEntry = { 0 };
		g_replacings[pNewAddress] = emptyEntry;
		auto& itNewEntry = g_replacings.find(pNewAddress);
		ReplacingEntry& newEntry = itNewEntry->second;

		_tcscpy_s(newEntry.line, line);
		newEntry.pOriginal = pOrgAddress;
		newEntry.pReplacing = pNewAddress;
		newEntry.pTrampoline = newEntry.pOriginal;

		DetourAttach(&newEntry.pTrampoline, newEntry.pReplacing);

		output.AppendFormat(_T("[OK] line=[%s], Original=0x%p, Replacing=0x%p, Trampoline=0x%p\n"), line, newEntry.pOriginal, newEntry.pReplacing, newEntry.pTrampoline);
	}

	DetourTransactionCommit();

	return true;
}

HRESULT UCTinker_Restore(const std::vector<CString>& parameters, CString& output)
{
	UC_LOG(_T("Undercover"), _T("Called Restore function."));

	DetourRestoreAfterWith();

	DetourTransactionBegin();

	DetourUpdateThread(GetCurrentThread());

	LONG result = NO_ERROR;
	for (auto& itEntry : g_replacings)
	{
		ReplacingEntry& entry = itEntry.second;
		result = DetourDetach(&entry.pTrampoline, entry.pReplacing);
		if (result != NO_ERROR)
		{
			CString message;
			message.Format(_T("[NG] line=[%s]\n"), entry.line);
			output.AppendFormat(message);
			UC_LOG(_T("Undercover"), message);
			break;
		}

		CString message;
		message.Format(_T("[OK] line=[%s]\n"), entry.line);
		output.AppendFormat(message);
		UC_LOG(_T("Undercover"), message);
	}

	if (result == NO_ERROR)
	{
		DetourTransactionCommit();
		g_replacings.clear();
	}
	else
	{
		DetourTransactionAbort();
	}

	return S_OK;
}

HRESULT UCTinker_CleanAll(const std::vector<CString>& parameters, CString& output)
{
	UC_LOG(_T("Undercover"), _T("Called CleanAll function."));
	return S_OK;
}

PVOID UCTinker_GetTrampoline(PVOID pNewFunction)
{
	auto& it = g_replacings.find(pNewFunction);
	if (it == g_replacings.end())
	{
		return nullptr;
	}

	return it->second.pTrampoline;
}