#pragma once
#include <atlstr.h>
#include <vector>
#include <map>
#include <functional>
#include "UC.CommandFunctionType.h"

typedef struct
{
	HMODULE handle;
	LPVOID baseAddress;
	TCHAR name[BUFSIZ];
	TCHAR path[BUFSIZ];
} UCModuleInfo;

#ifdef __cplusplus
extern "C"
{
#endif
	__declspec(dllexport) bool UCUtils_ConvertToULONGLONG(const CString& srcString, ULONGLONG& dstValue);
	__declspec(dllexport) void UCUtils_GetBaseDirectory(CString& baseDirectoryPath);
	__declspec(dllexport) void UCUtils_GetModuleInfoList(const HANDLE hProcess, std::map<CString, UCModuleInfo>& moduleInfoList);
	__declspec(dllexport) HRESULT UCUtils_ParseStdinLines(UCCommandFunction pUCCommandFunction, CString& output);
	__declspec(dllexport) HRESULT UCUtils_RunCommand(const std::vector<CString>& parameters, CString& output, std::function<HRESULT(const std::vector<CString>& parameters, CString& output)>);
	__declspec(dllexport) HRESULT UCUtils_Tokenize(const CString& input, const TCHAR separator, std::vector<CString>& tokens);
#ifdef __cplusplus
}
#endif
