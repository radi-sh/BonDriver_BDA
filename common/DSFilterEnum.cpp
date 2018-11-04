//------------------------------------------------------------------------------
// File: DSFilterEnum.cpp
//   Implementation of CDSFilterEnum class
//------------------------------------------------------------------------------

#include "common.h"

#include "DSFilterEnum.h"

#include <algorithm>

#include <DShow.h>

CDSFilterEnum::CDSFilterEnum(CLSID clsid)
	: CDSFilterEnum(clsid, 0)
{
}

CDSFilterEnum::CDSFilterEnum(CLSID clsid, DWORD dwFlags)
{
	HRESULT hr;

	if (FAILED(hr = m_pICreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC))) {
		std::wstring e(L"CDSFilterEnum: Error in CoCreateInstance.");
		throw e;
		return; /* not reached */
	}

	if ((FAILED(hr = m_pICreateDevEnum->CreateClassEnumerator(clsid, &m_pIEnumMoniker, dwFlags))) || (hr != S_OK)) {
		// CreateClassEnumerator ‚ªì‚ê‚È‚¢ || Œ©‚Â‚©‚ç‚È‚¢
		std::wstring e(L"CDSFilterEnum: Error in CreateClassEnumerator.");
		throw e;
		return; /* not reached */
	}

	return;
}

CDSFilterEnum::~CDSFilterEnum(void)
{
}

HRESULT CDSFilterEnum::next(void)
{
	m_pIMoniker.Release();

	HRESULT hr = m_pIEnumMoniker->Next(1, &m_pIMoniker, 0);

	return hr;
}


HRESULT CDSFilterEnum::getFilter(IBaseFilter ** ppFilter)
{
	if ((!ppFilter) || (m_pIMoniker == NULL))
		return E_POINTER;

	HRESULT hr;

	// bind the filter
    hr = m_pIMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **)ppFilter);

	return hr;
}

HRESULT CDSFilterEnum::getFilter(IBaseFilter ** ppFilter, ULONG order)
{
	if ((!ppFilter) || (m_pIEnumMoniker == NULL))
		return E_POINTER;

	m_pIMoniker.Release();

	m_pIEnumMoniker->Reset();

	HRESULT hr;

	if (order) {
		if (FAILED(hr = m_pIEnumMoniker->Skip(order))) {
			OutputDebug(L"CDSFilterEnum::getFilter: IEnumMoniker::Skip method failed.\n");
			return hr;
		}
	}

	if (FAILED(hr = m_pIEnumMoniker->Next(1, &m_pIMoniker, 0))) {
		OutputDebug(L"CDSFilterEnum::getFilter: IEnumMoniker::Next method failed.\n");
		return hr;
	}

	return getFilter(ppFilter);
}

HRESULT CDSFilterEnum::getFriendlyName(std::wstring * pName)
{
	if (m_pIMoniker == NULL) {
		return E_POINTER;
	}

    CComPtr<IPropertyBag> pBag;
	HRESULT hr;

	if(FAILED(hr = m_pIMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void**)&pBag))) {
		OutputDebug(L"CDSFilterEnum::getFriendlyName: IMoniker::BindToStorage method failed.\n");
        return hr;
    }

	VARIANT varName;
	::VariantInit(&varName);

	if(FAILED(hr = pBag->Read(L"FriendlyName", &varName, NULL))){
		OutputDebug(L"CDSFilterEnum::getFriendlyName: IPropertyBag::Read method failed.\n");
		::VariantClear(&varName);
		return hr;
    }
	
	*pName = varName.bstrVal;
	::VariantClear(&varName);

	return S_OK;
}

HRESULT CDSFilterEnum::getDisplayName(std::wstring * pName)
{
	HRESULT hr;
	if (m_pIMoniker == NULL) {
		return E_POINTER;
	}

	LPOLESTR pwszName;
	if (FAILED(hr = m_pIMoniker->GetDisplayName(NULL, NULL, &pwszName))) {
		return hr;
	}

	*pName = common::WStringToLowerCase(pwszName);

	return S_OK;
}

std::wstring CDSFilterEnum::getDeviceInstancePathrFromDisplayName(std::wstring displayName)
{
	std::wstring::size_type start = displayName.find(L"\\\\\?\\") + 4;
	std::wstring::size_type len = displayName.find_last_of(L"#") - start;
	std::wstring dip = common::WStringToUpperCase(displayName.substr(start, len));
	std::replace(dip.begin(), dip.end(), L'#', L'\\');

	return std::wstring(dip);
}
