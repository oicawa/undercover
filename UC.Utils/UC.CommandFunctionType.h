#pragma once

#include <vector>
#include <atlstr.h>
#include <functional>

typedef HRESULT(*UCCommandFunctionPointer)(const std::vector<CString>& parameters, CString& output);

using UCCommandFunction = std::function<HRESULT(const std::vector<CString>& parameters, CString& output)>;
