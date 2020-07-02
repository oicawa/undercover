#pragma once
#include <dia2.h>
#include <functional>

class CMsDiaHelper
{
public:
	CMsDiaHelper();
	~CMsDiaHelper();
	bool Init();
	IClassFactory& GetFactory();

private:
	HMODULE m_hMsDiaDll;
	IClassFactory* m_pFactory;
};

