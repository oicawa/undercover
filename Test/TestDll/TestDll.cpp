// TestDll.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "TestDll.h"
#include <UC.Logger\UC.Logger.h>
#include <UC.Tinker\UC.Tinker.h>

bool ReadLine2(CString& line)
{
	UC_LOG(_T("Undercover"), _T("Called ReadLine2(=0x%p) in TestDll.dll"), ReadLine2);

	typedef decltype(ReadLine2)* pReadLine2;
	pReadLine2 pTrampoline = (pReadLine2)UCTinker_GetTrampoline((PVOID)ReadLine2);
	if (pTrampoline == nullptr)
	{
		UC_LOG(_T("Undercover"), _T("UC_Tinker_GetTrampoline function Failed."));
		return false;
	}

	UC_LOG(_T("Undercover"), _T("Call Trampoline function. (0x%p)"), pTrampoline);
	return pTrampoline(line);
}

const CString& HogeFixed::GetName(bool isMr)
{
	CString modifiedName;
	modifiedName.Format(_T("%s%s"), isMr ? _T("Mr.") : _T("Miss "), m_name);
	return modifiedName;
};

bool ReadLine3(LPTSTR pBuffer, size_t count)
{
	UC_LOG(_T("Undercover"), _T("Called ReadLine2(=0x%p) in TestDll.dll"), ReadLine3);

	typedef decltype(ReadLine3)* pReadLine3;
	pReadLine3 pTrampoline = (pReadLine3)UCTinker_GetTrampoline((PVOID)ReadLine3);
	if (pTrampoline == nullptr)
	{
		UC_LOG(_T("Undercover"), _T("UC_Tinker_GetTrampoline function Failed."));
		return false;
	}

	UC_LOG(_T("Undercover"), _T("Call Trampoline function. (0x%p)"), pTrampoline);
	return pTrampoline(pBuffer, count);
}
