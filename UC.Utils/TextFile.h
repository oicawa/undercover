#pragma once

#include <stdio.h>
#include <atlstr.h>

class CTextFile
{
public:
	CTextFile(void) :
		m_mustFreeFile(false),
		m_pFile(nullptr)
	{
	};

	~CTextFile()
	{
		if (m_mustFreeFile && m_pFile)
		{
			fclose(m_pFile);
			m_pFile = nullptr;
		}
	};

	bool Open(LPCTSTR pFilePath)
	{
		m_mustFreeFile = true;
		errno_t error = _tfopen_s(&m_pFile, pFilePath, _T("r"));
		return m_pFile ? true : false;
	};

	bool Open(FILE* pFile, size_t maxBufferSize = BUFSIZ)
	{
		m_mustFreeFile = false;
		m_pFile = pFile;
		return true;
	};

	bool ReadLine(CString& line)
	{
		CString tmpLine;
		bool result = true;
		TCHAR buffer[BUFSIZ];
		size_t count = sizeof(buffer) / sizeof(TCHAR);
		bool isTerminatedByNewLine = false;
		while (true)
		{
			memset(buffer, NULL, sizeof(buffer));
			TCHAR* p = _fgetts(buffer, count, m_pFile);
			if (p == NULL)
			{
				result = false;
				break;
			}
			tmpLine.Append(buffer);

			int length = _tcslen(buffer);
			bool withNewLine = (buffer[length - 1] == _T('\n')) ? true : false;
			if (length < count - 1 || (0 < length) && withNewLine)
			{
				isTerminatedByNewLine = withNewLine;
				break;
			}
		}

		int totalLength = tmpLine.GetLength();
		line = totalLength == 0 ? _T("") : tmpLine.Mid(0, totalLength - (isTerminatedByNewLine ? 1 : 0));
		return result;
	};

private:
	bool m_mustFreeFile;
	FILE* m_pFile;
};

