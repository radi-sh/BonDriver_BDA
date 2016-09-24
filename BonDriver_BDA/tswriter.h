#pragma once

#include <streams.h>
#include <initguid.h>

// define a GUID for TsWriter filters
// {65DC13B3-6F96-4A33-8A87-826B0693DD25}
DEFINE_GUID(CLSID_TsWriter,
0x65dc13b3, 0x6f96, 0x4a33, 0x8a, 0x87, 0x82, 0x6b, 0x6, 0x93, 0xdd, 0x25);

// Class for the TsWriter filter
class CTsWriter: public CTransInPlaceFilter
{
protected:
	typedef int (CALLBACK * RECV_PROC) (void * pParam, BYTE * pbData, DWORD dwSize);

public:
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT * phr);

    CTsWriter(TCHAR * pName, LPUNKNOWN pUnk, HRESULT * hr);

	HRESULT Transform(IMediaSample * pSample);
	HRESULT CheckInputType(const CMediaType * mtIn);

	void SetCallBackRecv(RECV_PROC pRecv, void * pParam);
	HRESULT Write(PBYTE pbData, LONG lDataLength);

protected:
	RECV_PROC m_pRecv;
	void * m_pParam;
	CCritSec m_Lock;
};
