//------------------------------------------------------------------------------
// File: DSFilterEnum.cpp
//   Implementation of CDSFilterEnum class
//------------------------------------------------------------------------------

#include "common.h"

#include "DSFilterEnum.h"

#include <algorithm>

#include <DShow.h>

#pragma comment(lib, "strmiids")

CDSFilterEnum::CDSFilterEnum(CLSID clsid)
	: CDSFilterEnum(clsid, 0)
{
}

CDSFilterEnum::CDSFilterEnum(CLSID clsid, DWORD dwFlags)
{
	HRESULT hr;

	if (FAILED(hr = m_pICreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC))) {
		std::wstring e(L"CDSFilterEnum: Error in CoCreateInstance.");
		return;
	}

	if ((FAILED(hr = m_pICreateDevEnum->CreateClassEnumerator(clsid, &m_pIEnumMoniker, dwFlags))) || (hr != S_OK)) {
		// CreateClassEnumerator が作れない || 見つからない
		std::wstring e(L"CDSFilterEnum: Error in CreateClassEnumerator.");
		return;
	}

	return;
}

CDSFilterEnum::~CDSFilterEnum(void)
{
}

HRESULT CDSFilterEnum::next(void)
{
	if (!m_pIEnumMoniker) {
		return S_FALSE;
	}

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

	if(FAILED(hr = m_pIMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void **)&pBag))) {
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

std::wstring CDSFilterEnum::getRegistryName(IBaseFilter * pFilter)
{
	HRESULT hr;
	std::wstring strFilterName;
	// filterオブジェクトクラスのCLSIDを取得
	CComQIPtr<IPersist> pIpersist(pFilter);
	if (pIpersist) {
		CLSID clsidFilterObj;
		if (SUCCEEDED(hr = pIpersist->GetClassID(&clsidFilterObj))) {
			// CLSIDからレジストリキー名を作成
			std::wstring strRegKey(L"CLSID\\{" + common::GUIDToWString(clsidFilterObj) + L"}");
			// レジストリから名称を取得
			LSTATUS ret;
			HKEY hk;
			if ((ret = ::RegOpenKeyExW(HKEY_CLASSES_ROOT, strRegKey.c_str(), 0, KEY_READ, &hk)) == ERROR_SUCCESS) {
				WCHAR data[128];
				DWORD size = sizeof(data) / sizeof(data[0]);
				if ((ret = ::RegQueryValueExW(hk, NULL, NULL, NULL, (BYTE *)data, &size)) == ERROR_SUCCESS) {
					::RegCloseKey(hk);
					return std::wstring(data);
				}
				::RegCloseKey(hk);
			}
		}
	}
	return L"Unknown filter";
}

CDSEnumPins::CDSEnumPins(IBaseFilter * pFilter)
{
	HRESULT hr;
	if (FAILED(hr = pFilter->EnumPins(&m_pIEnumPins))) {
		OutputDebug(L"CDSEnumPins: Error in EnumPins.");
	}

	return;
}

CDSEnumPins::~CDSEnumPins(void)
{
}

HRESULT CDSEnumPins::getNextPin(IPin ** ppIPin)
{
	PIN_DIRECTION dir;
	return getNextPin(ppIPin, &dir);
}

HRESULT CDSEnumPins::getNextPin(IPin ** ppIPin, PIN_DIRECTION * pinDir)
{
	if (!m_pIEnumPins) {
		return S_FALSE;
	}

	HRESULT hr;
	if (S_OK != (hr = m_pIEnumPins->Next(1, ppIPin, NULL))) {
		return hr;
	}

	if (FAILED(hr = (*ppIPin)->QueryDirection(pinDir))) {
		OutputDebug(L"Can not get PinDir.");
		return hr;
	}
	return S_OK;
}

HRESULT CDSEnumPins::getNextPin(IPin ** ppIPin, PIN_DIRECTION pinDir)
{
	HRESULT hr;
	while (1) {
		CComPtr<IPin> pPin;
		PIN_DIRECTION dir;
		if (S_OK != (hr = getNextPin(&pPin, &dir))) {
			break;
		}

		if (pinDir == dir) {
			*ppIPin = pPin;
			(*ppIPin)->AddRef();
			break;
		}
	}

	return hr;
}

HRESULT CDSEnumPins::Reset(void)
{
	if (!m_pIEnumPins) {
		return S_OK;
	}

	return m_pIEnumPins->Reset();
}

CDSEnumNodes::CDSEnumNodes(IBaseFilter * pFilter)
	: m_pIBDA_Topology(pFilter),
	m_nNodeTypes(0),
	m_nNodeType()
{
	if (!m_pIBDA_Topology) {
		OutputDebug(L"CDSEnumNodes: Fail to get IBDA_Topology interface.\n");
		return;
	}

	HRESULT hr;

	if (FAILED(hr = m_pIBDA_Topology->GetNodeTypes(&m_nNodeTypes, MAX_NODES, m_nNodeType))) {
		OutputDebug(L"CDSEnumNodes: Fail to get NodeTypes.\n");
		return;
	}

	for (ULONG i = 0; i < m_nNodeTypes; i++) {
		ULONG Interfaces;
		GUID Interface[MAX_NODE_INTERFACES];
		if (FAILED(m_pIBDA_Topology->GetNodeInterfaces(m_nNodeType[i], &Interfaces, MAX_NODE_INTERFACES, Interface))) {
			OutputDebug(L"CDSEnumNodes: Fail to get GetNodeInterfaces for NodeType[%ld]=%ld.\n", i, m_nNodeType[i]);
		}
		else {
			for (ULONG j = 0; j < Interfaces; j++) {
				std::wstring sGuid(common::GUIDToWString(Interface[j]));
				OutputDebug(L"CDSEnumNodes: Found GUID=%s, NodeType[%ld]=%ld.\n", sGuid.c_str(), i, m_nNodeType[i]);
				m_Interfaces.emplace(sGuid, i);
			}
		}
	}
}

CDSEnumNodes::~CDSEnumNodes(void)
{
	m_Interfaces.clear();
}

ULONG CDSEnumNodes::getCount(void)
{
	return m_nNodeTypes;
}

ULONG CDSEnumNodes::getNodeType(ULONG index)
{
	return m_nNodeType[index];
}

HRESULT CDSEnumNodes::getControlNode(ULONG nodeType, IUnknown ** ppControlNode)
{
	if (!m_pIBDA_Topology) {
		return E_POINTER;
	}

	return m_pIBDA_Topology->GetControlNode(0UL, 1UL, nodeType, ppControlNode);
}

HRESULT CDSEnumNodes::getControlNode(GUID guidInterface, IUnknown ** ppControlNode)
{
	return getControlNodeFromNodeType(guidInterface, ppControlNode, (ULONG)-1);
}

HRESULT CDSEnumNodes::getControlNodeFromNodeType(GUID guidInterface, IUnknown ** ppControlNode, ULONG nodeType)
{
	return getControlNode(findControlNodeFromNodeType(guidInterface, nodeType), ppControlNode);
}

ULONG CDSEnumNodes::findControlNode(GUID guidInterface)
{
	return findControlNodeFromNodeType(guidInterface, (ULONG)-1);
}

ULONG CDSEnumNodes::findControlNodeFromNodeType(GUID guidInterface, ULONG nodeType)
{
	if (!m_pIBDA_Topology) {
		return (ULONG)-1;
	}

	auto range = m_Interfaces.equal_range(common::GUIDToWString(guidInterface));
	for (auto it = range.first; it != range.second; it++) {
		if (m_nNodeType[it->second] == nodeType || nodeType == (ULONG)-1) {
			return m_nNodeType[it->second];
		}
	}
	// not found
	return (ULONG)-1;
}

