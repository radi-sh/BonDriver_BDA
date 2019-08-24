#pragma once

#include "streams.h"
#include <initguid.h>

typedef int (CALLBACK * RECV_PROC)(void * pParam, BYTE * pbData, size_t size);

// define a IID for ITsWriter interface
// {57FE9CC4-DD3A-4FB3-9873-E7F3E41467D9}
DEFINE_GUID(IID_ITsWriter,
	0x57fe9cc4, 0xdd3a, 0x4fb3, 0x98, 0x73, 0xe7, 0xf3, 0xe4, 0x14, 0x67, 0xd9);

MIDL_INTERFACE("57fe9cc4-dd3a-4fb3-9873-e7f3e41467d9")
	ITsWriter: public IUnknown 
{
public:
	virtual STDMETHODIMP SetCallBackRecv(RECV_PROC pRecv, void * pParam) = 0;
};

// define a CLSID for CTsWriter filters
// {65DC13B3-6F96-4A33-8A87-826B0693DD25}
DEFINE_GUID(CLSID_TsWriter,
	0x65dc13b3, 0x6f96, 0x4a33, 0x8a, 0x87, 0x82, 0x6b, 0x6, 0x93, 0xdd, 0x25);

// Class for the TsWriter filter
class __declspec(uuid("65dc13b3-6f96-4a33-8a87-826b0693dd25"))
	CTsWriter: public CTransInPlaceFilter, public ITsWriter
{
public:
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT * phr);

    CTsWriter(LPCTSTR pName, LPUNKNOWN pUnk, HRESULT * phr);

	virtual HRESULT Transform(IMediaSample * pSample);
	virtual HRESULT CheckInputType(const CMediaType * mtIn);

	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
	virtual STDMETHODIMP SetCallBackRecv(RECV_PROC pRecv, void * pParam);

protected:
	HRESULT Write(PBYTE pbData, LONG lDataLength);

protected:
	RECV_PROC m_pRecv;
	void * m_pParam;
	CCritSec m_Lock;
};
