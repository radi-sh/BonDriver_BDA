//------------------------------------------------------------------------------
// File: DSFilterEnum.h
//   Define CDSFilterEnum class
//------------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <dshow.h>

#include <string>

using namespace std;

class CDSFilterEnum
{
public:
	CDSFilterEnum(CLSID clsID);
	CDSFilterEnum(CLSID clsID, DWORD dwFlags);
	virtual ~CDSFilterEnum(void);

	HRESULT next(void);
	HRESULT getFilter(IBaseFilter** ppFilter);
	HRESULT getFilter(IBaseFilter** ppFilter, ULONG order);
	HRESULT getFriendlyName(wstring* pName);
	HRESULT getDisplayName(wstring* pName);

private:
	IEnumMoniker* m_pIEnumMoniker;
	ICreateDevEnum* m_pICreateDevEnum;
	IMoniker* m_pIMoniker;
};
