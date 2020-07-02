#include "EnumSymbols.h"
#include <tchar.h>
#include <atlcomcli.h>

CEnumSymbols::CEnumSymbols() :
	m_pDataSource(NULL),
	m_pSession(NULL),
	m_pGlobal(NULL),
	m_pEnumSymbols(NULL)
{
}


CEnumSymbols::~CEnumSymbols()
{
	if (m_pEnumSymbols)
	{
		m_pEnumSymbols->Release();
		m_pEnumSymbols = NULL;
	}

	if (m_pGlobal)
	{
		m_pGlobal->Release();
		m_pGlobal = NULL;
	}

	if (m_pSession)
	{
		m_pSession->Release();
		m_pSession = NULL;
	}

	if (m_pDataSource)
	{
		m_pDataSource->Release();
		m_pDataSource = NULL;
	}
}

bool CEnumSymbols::Init(IClassFactory& factory, LPCTSTR pPdbPath, enum SymTagEnum symbolTag, LPCTSTR pFilter)
{
	HRESULT hr = S_OK;
	bool result = false;

	hr = factory.CreateInstance(NULL, __uuidof(IDiaDataSource), (void**)&m_pDataSource);
	if (FAILED(hr) || m_pDataSource == NULL)
	{
		_ftprintf(stderr, _T("IClassFactory::CreateInstance failed (hr=0x%08lx, pDataSource=0x%p, [%s])\n"), hr, m_pDataSource, pPdbPath);
		return false;
	}

	hr = m_pDataSource->loadDataFromPdb(pPdbPath);
	if (FAILED(hr))
	{
		_ftprintf(stderr, _T("IDiaDataSource::loadDataFromPdb failed (hr=0x%08lx, [%s])\n"), hr, pPdbPath);
		return false;
	}

	hr = m_pDataSource->openSession(&m_pSession);
	if (FAILED(hr))
	{
		_ftprintf(stderr, _T("IDiaDataSource::openSession failed (hr=0x%08lx, [%s])\n"), hr, pPdbPath);
		return false;
	}

	hr = m_pSession->get_globalScope(&m_pGlobal);
	if (FAILED(hr))
	{
		_ftprintf(stderr, _T("IDiaSession::get_globalScope failed (hr=0x%08lx, [%s])\n"), hr, pPdbPath);
		return false;
	}

	hr = m_pGlobal->findChildren(symbolTag, pFilter, NameSearchOptions::nsfRegularExpression, &m_pEnumSymbols);
	if (FAILED(hr) || m_pEnumSymbols == NULL)
	{
		_ftprintf(stderr, _T("IDiaSymbol::findChildren failed (hr=0x%08lx, [%s])\n"), hr, pPdbPath);
		return false;
	}

	return true;
}

void CEnumSymbols::Walkaround(std::function<bool(IDiaSymbol& symbol)> lambda)
{
	if (m_pEnumSymbols == NULL)
	{
		return;
	}

	for (;;)
	{
		IDiaSymbol *pSymbol = NULL;
		ULONG retrieved = 0;
		HRESULT hr = m_pEnumSymbols->Next(1, &pSymbol, &retrieved);
		if (FAILED(hr))
		{
			_ftprintf(stderr, _T("IDiaEnumSymbols::Next failed � hr=0x%08lx, pSymbol=0x%p\n"), hr, pSymbol);
			continue;
		}
		if (retrieved == 0 || pSymbol == nullptr)
		{
			break;
		}

		bool result = lambda(*pSymbol);

		pSymbol->Release();
		pSymbol = NULL;

		if (result)
		{
			break;
		}
	}
}
