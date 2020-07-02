#pragma once
#include "stdafx.h"
#include <functional>
#include <vector>
#include <UC.Logger\UC.Logger.h>
#include <atlstr.h>

class CPipe
{
public:
	CPipe(void)	:
		m_hNamedPipe(NULL),
		m_isRunning(false),
		m_maxBufferSize(0)
	{
	};
	~CPipe()
	{
		if (m_hNamedPipe)
		{
			UC_LOG(_T("Undercover"), _T("CloseHandle(g_hNamedPipe)"));
			CloseHandle(m_hNamedPipe);
			m_hNamedPipe = NULL;
		}
	};

	bool Create(DWORD processId, bool asServer, size_t maxBufferSize = BUFSIZ)
	{
		if (maxBufferSize == 0 || processId == 0)
		{
			UC_LOG(_T("Undercover"), _T("[NG] bufferSize=%Iu, processId=%lx"), maxBufferSize, processId);
			return false;
		}

		CString namedPipeName;
		namedPipeName.Format(_T("\\\\.\\pipe\\%lu\\UC.Agent"), processId);
		m_maxBufferSize = maxBufferSize;
		if (asServer)
		{
			m_hNamedPipe = CreateNamedPipe(namedPipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, m_maxBufferSize, m_maxBufferSize, 1000, NULL);
		}
		else
		{
			m_hNamedPipe = CreateFile(namedPipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		}

		bool result = (m_hNamedPipe == INVALID_HANDLE_VALUE) ? false : true;
		UC_LOG(_T("Undercover"), _T("[%s] m_maxBufferSize=%Iu, m_hNamedPipe=0x%p, namedPipeName=[%s]"), result ? _T("OK") : _T("NG"), m_maxBufferSize, m_hNamedPipe, (LPCTSTR)namedPipeName);
		return result;
	};
	bool Run(std::function<void(std::vector<BYTE>& input, std::vector<BYTE>& output)> lambda)
	{
		UC_LOG(_T("Undercover"), _T("m_hNamedPipe=0x%p"), m_hNamedPipe);
		if (m_hNamedPipe == NULL || m_hNamedPipe == INVALID_HANDLE_VALUE)
		{
			UC_LOG(_T("Undercover"), _T("[NG] m_hNamedPipe was invalid."));
			return false;
		}

		m_isRunning = true;
		while (m_isRunning)
		{
			ConnectNamedPipe(m_hNamedPipe, NULL);

			std::vector<byte> input;
			Read(input);

			if (input.size() == 0)
			{
				m_isRunning = false;
			}

			std::vector<byte> output;
			lambda(input, output);

			Write(output);

			DisconnectNamedPipe(m_hNamedPipe);
		}
		return true;
	};
	void Abort()
	{
		m_isRunning = false;
	};
	bool Read(std::vector<BYTE>& data)
	{
		UC_LOG(_T("Undercover"), _T("m_hNamedPipe=0x%p"), m_hNamedPipe);
		if (m_hNamedPipe == NULL || m_hNamedPipe == INVALID_HANDLE_VALUE)
		{
			UC_LOG(_T("Undercover"), _T("[NG] m_hNamedPipe was invalid."));
			return false;
		}

		if (data.capacity() < m_maxBufferSize)
		{
			data.reserve(m_maxBufferSize);
		}
		data.empty();

		BOOL result = false;

		size_t size = 0;
		DWORD numberOfBytesRead = 0;
		UC_LOG(_T("Undercover"), _T(">> ReadFile(...) for size"));
		result = ReadFile(m_hNamedPipe, &size, sizeof(size_t), &numberOfBytesRead, NULL);
		UC_LOG(_T("Undercover"), _T("<< ReadFile(...), result=%d, numberOfBytesRead=%lu, size=%Iu"), result, numberOfBytesRead, size);


		byte* pBuffer = (byte*)malloc(m_maxBufferSize);
		if (pBuffer == NULL)
		{
			return false;
		}

		UC_LOG(_T("Undercover"), _T(">> while loop"));
		while (data.size() < size)
		{
			ZeroMemory(pBuffer, m_maxBufferSize);
			numberOfBytesRead = 0;
			UC_LOG(_T("Undercover"), _T(">> ReadFile(...)"));
			result = ReadFile(m_hNamedPipe, pBuffer, m_maxBufferSize, &numberOfBytesRead, NULL);
			UC_LOG(_T("Undercover"), _T("<< ReadFile(...), result=%d, numberOfBytesRead=%lu"), result, numberOfBytesRead);
			if (result == FALSE)
			{
				UC_LOG(_T("Undercover"), _T("[NG] ReadFile(...) was Failed."));
				break;
			}

			for (size_t i = 0; i < numberOfBytesRead; i++)
			{
				data.push_back(pBuffer[i]);
			}
		}
		UC_LOG(_T("Undercover"), _T("<< while loop"));

		data.resize(size);

		free(pBuffer);
		pBuffer = NULL;

		return result ? true : false;
	};
	bool Write(std::vector<BYTE>& data)
	{
		UC_LOG(_T("Undercover"), _T("m_hNamedPipe=0x%p"), m_hNamedPipe);
		if (m_hNamedPipe == NULL || m_hNamedPipe == INVALID_HANDLE_VALUE)
		{
			UC_LOG(_T("Undercover"), _T("[NG] m_hNamedPipe was invalid."));
			return false;
		}

		BOOL result = FALSE;

		UC_LOG(_T("Undercover"), _T(">> WriteFile(...) for data.size=%Iu"), data.size());
		size_t size = data.size();
		DWORD numberOfBytesWritten = 0;
		result = WriteFile(m_hNamedPipe, &size, sizeof(size_t), &numberOfBytesWritten, NULL);
		UC_LOG(_T("Undercover"), _T("<< WriteFile(...), result=%d, numberOfBytesWritten=%lu"), result, numberOfBytesWritten);


		size_t cursor = 0;
		UC_LOG(_T("Undercover"), _T(">> while loop, data.size()=%Iu"), data.size());
		while (cursor < data.size())
		{
			UC_LOG(_T("Undercover"), _T(">> WriteFile(...), cursor=%Iu"), cursor);
			numberOfBytesWritten = 0;
			result = WriteFile(m_hNamedPipe, &data.data()[cursor], m_maxBufferSize, &numberOfBytesWritten, NULL);
			UC_LOG(_T("Undercover"), _T("<< WriteFile(...), result=%d, numberOfBytesWritten=%lu"), result, numberOfBytesWritten);
			if (result == FALSE)
			{
				UC_LOG(_T("Undercover"), _T("[NG] WriteFile(...) was Failed."));
				break;
			}
			cursor += m_maxBufferSize;
		}
		UC_LOG(_T("Undercover"), _T("<< while loop"));

		return result ? true : false;
	};

private:
	HANDLE m_hNamedPipe;
	bool m_isRunning;
	size_t m_maxBufferSize;

};

