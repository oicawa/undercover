#include "MsDiaHelper.h"
#include <WinBase.h>
#include <Shlwapi.h>
#include <tchar.h>

CMsDiaHelper::CMsDiaHelper() :
	m_hMsDiaDll(NULL),
	m_pFactory(NULL)
{
}

CMsDiaHelper::~CMsDiaHelper()
{
	if (m_pFactory)
	{
		m_pFactory->Release();
		m_pFactory = NULL;
	}
	if (m_hMsDiaDll)
	{
		FreeLibrary(m_hMsDiaDll);
		m_hMsDiaDll = NULL;
	}
}

bool CMsDiaHelper::Init()
{
	TCHAR msdiaPath[MAX_PATH];
	ZeroMemory(msdiaPath, sizeof(msdiaPath));
	GetModuleFileName(NULL, msdiaPath, MAX_PATH);
	PathRemoveFileSpec(msdiaPath);
	PathAppend(msdiaPath, _T("msdia140.dll"));

	m_hMsDiaDll = LoadLibrary(msdiaPath);
	if (m_hMsDiaDll == nullptr)
	{
		_ftprintf(stderr, _T("LoadLibrary failed � 0x%08lx\n"), GetLastError());
		return false;
	}

	typedef HRESULT(__stdcall *DllGetClassObjectFunctionPointer)(
		_In_   REFCLSID rclsid,
		_In_   REFIID riid,
		_Out_  LPVOID *ppv
		);

	DllGetClassObjectFunctionPointer pDllGetClassObject = (DllGetClassObjectFunctionPointer)GetProcAddress(m_hMsDiaDll, "DllGetClassObject");
	if (pDllGetClassObject == nullptr)
	{
		_ftprintf(stderr, _T("LoadLibrary failed � 0x%08lx\n"), GetLastError());
		return false;
	}

	HRESULT hr = S_OK;

	hr = pDllGetClassObject(__uuidof(DiaSource), __uuidof(IClassFactory), (void**)&m_pFactory);
	if (FAILED(hr))
	{
		_ftprintf(stderr, _T("DllGetClassObject failed � 0x%08lx\n"), hr);
		return false;
	}

	return true;
}

IClassFactory& CMsDiaHelper::GetFactory()
{
	return *m_pFactory;
}
