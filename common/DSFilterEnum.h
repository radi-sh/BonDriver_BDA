//------------------------------------------------------------------------------
// File: DSFilterEnum.h
//   Define CDSFilterEnum class
//------------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <dshow.h>

#include <string>

class CDSFilterEnum
{
public:
	CDSFilterEnum(CLSID clsID);
	CDSFilterEnum(CLSID clsID, DWORD dwFlags);
	virtual ~CDSFilterEnum(void);

	HRESULT next(void);
	HRESULT getFilter(IBaseFilter** ppFilter);
	HRESULT getFilter(IBaseFilter** ppFilter, ULONG order);
	HRESULT getFriendlyName(std::wstring* pName);
	HRESULT getDisplayName(std::wstring* pName);

private:
	IEnumMoniker* m_pIEnumMoniker;
	ICreateDevEnum* m_pICreateDevEnum;
	IMoniker* m_pIMoniker;
};
