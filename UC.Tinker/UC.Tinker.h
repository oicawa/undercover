#pragma once
#include <atlstr.h>
#include <vector>

#ifdef __cplusplus
extern "C"
{
#endif
	__declspec(dllexport) HRESULT UCTinker_List(const std::vector<CString>& parameters, CString& output);
	__declspec(dllexport) HRESULT UCTinker_Replace(const std::vector<CString>& parameters, CString& output);
	__declspec(dllexport) HRESULT UCTinker_Restore(const std::vector<CString>& parameters, CString& output);
	//__declspec(dllexport) HRESULT UCTinker_Insert(const std::vector<CString>& parameters, CString& output);
	//__declspec(dllexport) HRESULT UCTinker_Remove(const std::vector<CString>& parameters, CString& output);
	__declspec(dllexport) PVOID UCTinker_GetTrampoline(PVOID pNewFunction);
#ifdef __cplusplus
}
#endif
