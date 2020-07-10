#include "Privilege.h"
#include <handleapi.h>
#include <processthreadsapi.h>
#include <errhandlingapi.h>
#include <winerror.h>
#include <securitybaseapi.h>

#include "..\UC.Logger\UC.Logger.h"

CPrivilege::CPrivilege() :
	m_hToken(nullptr)
{
}

CPrivilege::~CPrivilege()
{
	Set(m_hToken, SE_DEBUG_NAME, false);

	if (m_hToken)
	{
		CloseHandle(m_hToken);
		m_hToken = nullptr;
	}
}

bool CPrivilege::Init()
{
	if (OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &m_hToken) == TRUE)
	{
		Set(m_hToken, SE_DEBUG_NAME, true);
		return true;
	}

	if (GetLastError() != ERROR_NO_TOKEN)
	{
		return false;
	}

	// ERROR_NO_TOKEN

	if (!ImpersonateSelf(SecurityImpersonation))
	{
		return false;
	}

	if (OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &m_hToken) == TRUE)
	{
		Set(m_hToken, SE_DEBUG_NAME, true);
		return true;
	}

	UC_LOG(L"Undercover", L"[NG] OpenThreadToken");
	return false;
}

bool CPrivilege::Set(const HANDLE hToken, LPCTSTR privilege, const bool enable)
{
	TOKEN_PRIVILEGES tokenPrivileges;
	LUID luid;
	TOKEN_PRIVILEGES tokenPrivilegesPrev;
	DWORD returnLength = sizeof(TOKEN_PRIVILEGES);

	if (!LookupPrivilegeValue(NULL, privilege, &luid))
	{
		return false;
	}

	// first pass.  get current privilege setting
	tokenPrivileges.PrivilegeCount = 1;
	tokenPrivileges.Privileges[0].Luid = luid;
	tokenPrivileges.Privileges[0].Attributes = 0;

	AdjustTokenPrivileges(hToken, FALSE, &tokenPrivileges, sizeof(TOKEN_PRIVILEGES), &tokenPrivilegesPrev, &returnLength);

	if (GetLastError() != ERROR_SUCCESS)
	{
		return false;
	}

	// second pass.  set privilege based on previous setting
	tokenPrivilegesPrev.PrivilegeCount = 1;
	tokenPrivilegesPrev.Privileges[0].Luid = luid;

	if (enable)
	{
		tokenPrivilegesPrev.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
	}
	else
	{
		tokenPrivilegesPrev.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED & tokenPrivilegesPrev.Privileges[0].Attributes);
	}

	AdjustTokenPrivileges(hToken, FALSE, &tokenPrivilegesPrev, returnLength, NULL, NULL);

	if (GetLastError() != ERROR_SUCCESS)
	{
		return false;
	}

	return true;

}