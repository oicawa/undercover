// TestExe.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <atlstr.h>

class Hoge
{
public:
	Hoge(CString name, DWORD year) :
		m_name(name),
		m_year(year)
	{};
	const DWORD GetYear() { return m_year; };

	const CString& GetName() { return m_name; };

private:
	CString m_name;
	DWORD m_year;

public:
	const CString& GetName(bool isMr)
	{
		CString modifiedName;
		modifiedName.Format(_T("%s%s"), isMr ? _T("Mr.") : _T("Mrs."), m_name);
		return modifiedName;
	};
	void SetName(const CString name)
	{
		m_name = name;
	};
};

bool ReadLine(CString& line)
{
	CString tmpLine;
	bool result = true;
	TCHAR buffer[3];
	size_t count = sizeof(buffer) / sizeof(TCHAR);
	while (true)
	{
		memset(buffer, NULL, sizeof(buffer));
		TCHAR* p = _fgetts(buffer, count, stdin);
		if (p == NULL)
		{
			result = false;
			break;
		}
		tmpLine.Append(buffer);

		int length = _tcslen(buffer);
		if (length < count - 1 || (0 < length) && buffer[length - 1] == _T('\n'))
		{
			break;
		}
	}

	int totalLength = tmpLine.GetLength();
	line = totalLength == 0 ? _T("") : tmpLine.Mid(0, totalLength - 1);
	CString log;
	log.Format(_T("Undercover, line=[%s]"), line);
	OutputDebugString(log);
	return result;
}

int main()
{
	_tprintf(_T("Process ID : %lu\n"), GetCurrentProcessId());
	_tprintf(_T("ReadLine   : 0x%p\n"), (void*)ReadLine);

	Hoge test(_T("test"), 2);
	_tprintf(_T("test.year  : [%lu]\n"), test.GetYear());
	_tprintf(_T("test.name  : [%s]\n"), test.GetName());

	while (true)
	{
		_tprintf(_T(">> "));
		CString line;
		bool result = ReadLine(line);
		if (result == false)
		{
			_ftprintf(stderr, _T("[NG] _fgetts was failed\n"));
			break;
		}
		if (line.CompareNoCase(_T("quit")) == 0)
		{
			_tprintf(_T("Quit TestExe...\n"));
			break;
		}

		_tprintf(_T("Input String = [%s]\n"), (LPCTSTR)line);

		_tprintf(_T("(test.name  : [%s])\n"), test.GetName(false));
	}
	return 0;
}

