//------------------------------------------------------------------------------
// File: DSFilterEnum.cpp
//   Implementation of CDSFilterEnum class
//------------------------------------------------------------------------------

#include "DSFilterEnum.h"

#include "common.h"

#include <iostream>
#include <DShow.h>

// transform()
#include <algorithm>

using namespace std;

CDSFilterEnum::CDSFilterEnum(CLSID clsid)
	: CDSFilterEnum(clsid, 0)
{
}

CDSFilterEnum::CDSFilterEnum(CLSID clsid, DWORD dwFlags)
	: m_pIEnumMoniker(NULL), 
	  m_pICreateDevEnum(NULL),
	  m_pIMoniker(NULL)
{
	HRESULT hr = ::CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC, IID_ICreateDevEnum, reinterpret_cast<void**>(&m_pICreateDevEnum));

	if (FAILED(hr)) {
		wstring e(L"Error in CoCreateInstance.");
		throw e;
		return; /* not reached */
	}

	hr = m_pICreateDevEnum->CreateClassEnumerator(clsid, &m_pIEnumMoniker, dwFlags);

	if ((FAILED (hr)) || (hr != S_OK)) {
		// CreateClassEnumerator ‚ªì‚ê‚È‚¢ || Œ©‚Â‚©‚ç‚È‚¢
		SAFE_RELEASE(m_pICreateDevEnum);
		wstring e(L"Error in CreateClassEnumerator.");
		throw e;
		return; /* not reached */
	}

	return;
}

CDSFilterEnum::~CDSFilterEnum(void)
{
	SAFE_RELEASE(m_pIMoniker);
	SAFE_RELEASE(m_pICreateDevEnum);
	SAFE_RELEASE(m_pIEnumMoniker);
}

HRESULT CDSFilterEnum::next(void)
{
	SAFE_RELEASE(m_pIMoniker);

	HRESULT hr = m_pIEnumMoniker->Next(1, &m_pIMoniker, 0);

	return hr;
}


HRESULT CDSFilterEnum::getFilter(IBaseFilter** ppFilter)
{
	if ((!ppFilter) || (m_pIMoniker == NULL))
		return E_POINTER;

	*ppFilter = NULL;

	// bind the filter        
    HRESULT hr = m_pIMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, reinterpret_cast<void**>(ppFilter));

	if (!*ppFilter)
			return E_POINTER;

	return hr;
}

HRESULT CDSFilterEnum::getFilter(IBaseFilter** ppFilter, ULONG order)
{
	if ((!ppFilter) || (m_pIEnumMoniker == NULL))
		return E_POINTER;

	SAFE_RELEASE(m_pIMoniker);
	*ppFilter = NULL;

	HRESULT hr;
		
	m_pIEnumMoniker->Reset();

	if (order) {
		hr = m_pIEnumMoniker->Skip(order);
		if (FAILED(hr)) {
			OutputDebug(L"m_pIEnumMoniker->Skip method failed.\n");
			return hr;
		}
	}

	hr = m_pIEnumMoniker->Next(1, &m_pIMoniker, 0);
	if (FAILED(hr)) {
		OutputDebug(L"m_pIEnumMoniker->Next method failed.\n");
		return hr;
	}

	return getFilter(ppFilter);
}

HRESULT CDSFilterEnum::getFriendlyName(wstring* pName)
{
	if (m_pIMoniker == NULL) {
		return E_POINTER;
	}

    IPropertyBag *pBag;
    HRESULT hr =m_pIMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, reinterpret_cast<void**>(&pBag));

	if(FAILED(hr)) {
		OutputDebug(L"Cannot BindToStorage for.\n");
        return hr;
    }

	VARIANT varName;
	::VariantInit(&varName);

	hr = pBag->Read(L"FriendlyName", &varName, NULL);

	if(FAILED(hr)){
		OutputDebug(L"IPropertyBag->Read method failed for.\n");
		SAFE_RELEASE(pBag);
		::VariantClear(&varName);
		return hr;
    }
	
	*pName = varName.bstrVal;
	
	SAFE_RELEASE(pBag);
	::VariantClear(&varName);

	return S_OK;
}

HRESULT CDSFilterEnum::getDisplayName(wstring* pName)
{
	HRESULT hr;
	if (m_pIMoniker == NULL) {
		return E_POINTER;
	}

	LPOLESTR pwszName;
	if (FAILED(hr = m_pIMoniker->GetDisplayName(NULL, NULL, &pwszName))) {
		return hr;
	}

	*pName = pwszName;
	::transform(pName->begin(), pName->end(), pName->begin(), towlower);

	return S_OK;
}
