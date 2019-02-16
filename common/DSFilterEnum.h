//------------------------------------------------------------------------------
// File: DSFilterEnum.h
//   Define CDSFilterEnum class
//------------------------------------------------------------------------------

#pragma once

#include <dshow.h>
#include <bdaiface.h>

#include <string>
#include <map>
#include <atlbase.h>

class CDSFilterEnum
{
public:
	CDSFilterEnum(CLSID clsID);
	CDSFilterEnum(CLSID clsID, DWORD dwFlags);
	virtual ~CDSFilterEnum(void);

	HRESULT next(void);
	HRESULT getFilter(IBaseFilter ** ppFilter);
	HRESULT getFilter(IBaseFilter ** ppFilter, ULONG order);
	HRESULT getFriendlyName(std::wstring * pName);
	HRESULT getDisplayName(std::wstring * pName);

	static std::wstring getDeviceInstancePathrFromDisplayName(std::wstring Name);
	static std::wstring getRegistryName(IBaseFilter * pFilter);

private:
	CComPtr<IEnumMoniker> m_pIEnumMoniker;
	CComPtr<ICreateDevEnum> m_pICreateDevEnum;
	CComPtr<IMoniker> m_pIMoniker;
};

class CDSEnumPins
{
public:
	CDSEnumPins(IBaseFilter * pFilter);
	virtual ~CDSEnumPins(void);

	HRESULT getNextPin(IPin ** ppIPin);
	HRESULT getNextPin(IPin ** ppIPin, PIN_DIRECTION * pinDir);
	HRESULT getNextPin(IPin ** ppIPin, PIN_DIRECTION pinDir);
	HRESULT Reset(void);

private:
	CComPtr<IEnumPins> m_pIEnumPins;
};

class CDSEnumNodes
{
public:
	CDSEnumNodes(IBaseFilter * pFilter);
	virtual ~CDSEnumNodes(void);

	ULONG getCount(void);
	ULONG getNodeType(ULONG index);
	HRESULT getControlNode(ULONG nodeType, IUnknown ** ppControlNode);
	HRESULT getControlNode(GUID guidInterface, IUnknown ** ppControlNode);
	HRESULT getControlNodeFromNodeType(GUID guidInterface, IUnknown ** ppControlNode, ULONG nodeType);
	ULONG findControlNode(GUID guidInterface);
	ULONG findControlNodeFromNodeType(GUID guidInterface, ULONG nodeType);

private:
	CComQIPtr<IBDA_Topology> m_pIBDA_Topology;
	static constexpr size_t MAX_NODES = 32;
	static constexpr size_t MAX_NODE_INTERFACES = 32;
	ULONG m_nNodeTypes;
	ULONG m_nNodeType[MAX_NODES];
	std::multimap<std::wstring, ULONG, std::less<>> m_Interfaces;
};