#include "tswriter.h"

CUnknown * WINAPI CTsWriter::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
    return new CTsWriter(NAME("TS Writer Filter"), pUnk, phr);
}

CTsWriter::CTsWriter(TCHAR * pName, LPUNKNOWN pUnk, HRESULT * phr)
	: CTransInPlaceFilter(pName, pUnk, CLSID_TsWriter, phr),
	  m_pRecv(NULL),
	  m_pParam(NULL)
{
}

HRESULT CTsWriter::Transform(IMediaSample * pSample)
{
	PBYTE pbData;
	HRESULT hr = pSample->GetPointer(&pbData);
	if (SUCCEEDED(hr)) {
		Write(pbData, pSample->GetActualDataLength());
	}

	return S_OK;
}

HRESULT CTsWriter::CheckInputType(const CMediaType * mtIn)
{
	return S_OK;
}

void CTsWriter::SetCallBackRecv(RECV_PROC pRecv, void * pParam)
{
	CAutoLock lock_it(&m_Lock);
	m_pRecv = pRecv;
	m_pParam = pParam;
}

HRESULT CTsWriter::Write(PBYTE pbData, LONG lDataLength)
{
	if (lDataLength < 188) {
		return S_OK;
	}

	{
		CAutoLock lock_it(&m_Lock);
		if (m_pRecv != NULL) {
			m_pRecv(m_pParam, pbData, lDataLength);
		}
	}
    return S_OK;
}
