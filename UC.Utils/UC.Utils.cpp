// UC.Utils.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "UC.Utils.h"
#include <tchar.h>
#include <psapi.h>
#pragma comment(lib,"psapi.lib")
#include <intsafe.h>
#include <winternl.h>
#include "UC.CommandFunctionType.h"
#include "TextFile.h"
#include <UC.Logger\UC.Logger.h>

bool UCUtils_ConvertToULONGLONG(const CString& srcString, ULONGLONG& dstValue)
{
	bool isHex = (srcString.Left(2).CompareNoCase(_T("0x"))) == 0 ? true : false;

	TCHAR* pEnd = NULL;
	dstValue = _tcstoull(srcString, &pEnd, isHex ? 16 : 10);
	if (*pEnd != '\0')
	{
		return false;
	}

	if (ULONGLONG_MAX < dstValue)
	{
		return false;
	}

	CString checkString;
	bool result = false;
	if (isHex)
	{
		checkString.Format(L"%llx", dstValue);
		CString srcTmpString = srcString;
		srcTmpString.Trim();
		srcTmpString.TrimLeft(_T("0xX"));
		srcTmpString = srcTmpString.IsEmpty() ? _T("0") : srcTmpString;
		result = (checkString.CompareNoCase(srcTmpString) == 0) ? true : false;
	}
	else
	{
		checkString.Format(L"%llu", dstValue);
		result = (checkString.CompareNoCase(srcString) == 0) ? true : false;
	}

	return result;
}

void UCUtils_GetBaseDirectory(CString& baseDirectoryPath)
{
	TCHAR buffer[MAX_PATH];
	ZeroMemory(buffer, sizeof(buffer));
	GetModuleFileName(nullptr, buffer, MAX_PATH);
	PathRemoveFileSpec(buffer);
	baseDirectoryPath = buffer;
}

void UCUtils_GetModuleInfoList(const HANDLE hProcess, std::map<CString, UCModuleInfo>& moduleInfoList)
{
	// Get size only
	DWORD size = 0;
	EnumProcessModules(hProcess, nullptr, 0, &size);
	UC_LOG(_T("Undercover"), _T("size=%lu"), size);

	// Get Module Handles
	std::vector<HMODULE> moduleHandles;
	moduleHandles.resize(size);
	EnumProcessModules(hProcess, moduleHandles.data(), size, &size);

	for (auto& hModule : moduleHandles)
	{
		if (hModule == nullptr)
		{
			continue;
		}
		MODULEINFO moduleInfo = { 0 };
		GetModuleInformation(hProcess, hModule, &moduleInfo, sizeof(moduleInfo));

		UCModuleInfo ucModuleInfo = { 0 };
		ucModuleInfo.handle = hModule;
		ucModuleInfo.baseAddress = moduleInfo.lpBaseOfDll;
		GetModuleBaseName(hProcess, hModule, ucModuleInfo.name, sizeof(ucModuleInfo.name));
		GetModuleFileNameEx(hProcess, hModule, ucModuleInfo.path, sizeof(ucModuleInfo.path));
		UC_LOG(_T("Undercover"), _T("ucModuleInfo.{handle=0x%p, baseAddress=0x%p, name=[%-16s], path=[%s]}"), ucModuleInfo.handle, ucModuleInfo.baseAddress, ucModuleInfo.name, ucModuleInfo.path);

		moduleInfoList[ucModuleInfo.path] = ucModuleInfo;
	}
}

HRESULT UCUtils_ParseStdinLines(UCCommandFunction pUCCommandFunction, CString& output)
{
	CTextFile file;
	file.Open(stdin);
	while (true)
	{
		CString line;
		bool result = file.ReadLine(line);
		if (result == false)
		{
			break;
		}

		std::vector<CString> parameters;
		int cursor = 0;
		while (true)
		{
			int end = line.Find(_T(' '), cursor);
			CString parameter = (end < 0) ? line.Mid(cursor) : line.Mid(cursor, end - cursor);
			parameters.push_back(parameter);
			if (end < 0)
			{
				break;
			}
			cursor = end + 1;
		}

		pUCCommandFunction(parameters, output);
	}

	return S_OK;
}

HRESULT UCUtils_RunCommand(const std::vector<CString>& parameters, CString& output, std::function<HRESULT(const std::vector<CString>& parameters, CString& output)> pUCCommandFunction)
{
	if (0 < parameters.size())
	{
		// Use command line arguments
		return pUCCommandFunction(parameters, output);
	}

	CTextFile file;
	file.Open(stdin);
	while (true)
	{
		CString line;
		bool result = file.ReadLine(line);
		UC_LOG(_T("Undercover"), _T("result=%d, line=[%s]"), result, line);
		if (result == false)
		{
			break;
		}

		std::vector<CString> parameters;
		int cursor = 0;
		while (true)
		{
			int end = line.Find(_T(' '), cursor);
			CString parameter = (end < 0) ? line.Mid(cursor) : line.Mid(cursor, end - cursor);
			UC_LOG(_T("Undercover"), _T("  parameter=[%s]"), parameter);
			parameters.push_back(parameter);
			if (end < 0)
			{
				break;
			}
			cursor = end + 1;
		}

		pUCCommandFunction(parameters, output);
	}

	return S_OK;
}

HRESULT UCUtils_Tokenize(const CString& input, const TCHAR separator, std::vector<CString>& tokens)
{
	CString buffer = input;
	int cursor = 0;
	while (true)
	{
		buffer = buffer.TrimLeft();
		int index = buffer.Find(separator, cursor);
		CString token = (index < 0) ? buffer.Mid(cursor).Trim() : buffer.Mid(cursor, index - cursor).Trim();
		if (token.IsEmpty() == false)
		{
			tokens.push_back(token);
		}
		if (index < 0)
		{
			break;
		}
		cursor = index + 1;
	}

	return (tokens.size() == 0) ? E_INVALIDARG : S_OK;
}
