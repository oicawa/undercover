#pragma once
#include <atlstr.h>

__declspec(dllexport) bool ReadLine2(CString& line);
__declspec(dllexport) bool ReadLine2(CString& line);
class __declspec(dllexport) HogeFixed
{
private:
	CString m_name;
	DWORD m_year;
public:
	const CString& GetName(bool isMr);
};
