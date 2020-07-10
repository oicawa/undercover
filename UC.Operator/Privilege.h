#pragma once
#include <atlstr.h>

class CPrivilege
{
public:
	CPrivilege();
	~CPrivilege();

public:
	bool Init();

private:
	bool Set(const HANDLE hToken, LPCTSTR privilege, const bool enable);

private:
	HANDLE m_hToken;
};

