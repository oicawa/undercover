#pragma once
#include <dia2.h>
#include <functional>

class CEnumSymbols
{
public:
	CEnumSymbols();
	~CEnumSymbols();

	bool Init(IClassFactory& factory, LPCTSTR pPdbPath, enum SymTagEnum symbolTag, LPCTSTR pFilter);
	void Walkaround(std::function<bool(IDiaSymbol& symbol)> lambda);

private:
	IDiaDataSource* m_pDataSource;
	IDiaSession* m_pSession;
	IDiaSymbol* m_pGlobal;
	IDiaEnumSymbols* m_pEnumSymbols;

};

