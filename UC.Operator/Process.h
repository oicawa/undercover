#pragma once

#include "stdafx.h"
#include <atlstr.h>
#include <vector>

typedef struct {
	DWORD id;
	TCHAR name[BUFSIZ];
	TCHAR commandLine[BUFSIZ];
} UCProcessInfo;

HRESULT GetProcessInfoList(LPCTSTR pInputString, std::vector<UCProcessInfo>& processInfoList);
