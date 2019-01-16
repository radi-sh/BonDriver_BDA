#include "tswriter.h"

CUnknown * WINAPI CTsWriter::CreateInstance(LPUNKNOWN pUnk, HRESULT * phr)
{
	CTsWriter *pFilter = new CTsWriter(NAME("TS Writer Filter"), pUnk, phr);
	if (pFilter == NULL)
	{
		*phr = E_OUTOFMEMORY;
	}
	return pFilter;
}

CTsWriter::CTsWriter(LPCTSTR pName, LPUNKNOWN pUnk, HRESULT * phr)
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

STDMETHODIMP CTsWriter::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
	if (riid == IID_ITsWriter) {
		return GetInterface((ITsWriter *)this, ppv);
	}
	return CTransformFilter::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CTsWriter::SetCallBackRecv(RECV_PROC pRecv, void * pParam)
{
	CAutoLock lock_it(&m_Lock);
	m_pRecv = pRecv;
	m_pParam = pParam;
	return S_OK;
}

HRESULT CTsWriter::Write(PBYTE pbData, LONG lDataLength)
{
	if (lDataLength < 188) {
		return S_OK;
	}

	{
		CAutoLock lock_it(&m_Lock);
		if (m_pRecv != NULL) {
			m_pRecv(m_pParam, pbData, (size_t)lDataLength);
		}
	}
    return S_OK;
}
