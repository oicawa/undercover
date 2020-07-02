#pragma once
#include <vector>
#include <atlstr.h>

#ifdef __cplusplus
extern "C"
{
#endif
	__declspec(dllexport) HRESULT RemoveModule(const std::vector<CString>& parameters, CString& output);
	__declspec(dllexport) HRESULT HogeHoge(const std::vector<CString>& parameters, CString& output);
#ifdef __cplusplus
}
#endif
