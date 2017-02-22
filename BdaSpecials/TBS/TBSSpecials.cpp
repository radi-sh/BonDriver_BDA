//------------------------------------------------------------------------------
// File: TBSSpecials.cpp
//   Implementation of CTBSSpecials class
//------------------------------------------------------------------------------

#include <Windows.h>
#include <stdio.h>

#include <string>

#include "TBSSpecials.h"

#include <iostream>
#include <dshow.h>

#include "common.h"

#pragma comment( lib, "Strmiids.lib" )

FILE *g_fpLog = NULL;

// Module handle (global)
/////////////////////////////////////////////

HMODULE hMySelf;

// DllMain
/////////////////////////////////////////////

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	hMySelf = hModule;
    return TRUE;
}

// CreateBdaSpecials(void) method
/////////////////////////////////////////////
__declspec(dllexport) IBdaSpecials * CreateBdaSpecials(CComPtr<IBaseFilter> pTunerDevice)
{
	return new CTBSSpecials(hMySelf, pTunerDevice);
}

// Constructor
/////////////////////////////////////

CTBSSpecials::CTBSSpecials(HMODULE hMySelf, CComPtr<IBaseFilter> pTunerDevice)
: m_hMySelf(hMySelf), m_pTunerDevice(pTunerDevice), m_pPropsetTunerPin(NULL)
{
	return;
}

// Destructor
/////////////////////////////////////

CTBSSpecials::~CTBSSpecials()
{
	m_hMySelf = NULL;
	if (m_pTunerDevice) {
		m_pTunerDevice.Release();
		m_pTunerDevice = NULL; 
	}
	if (m_pPropsetTunerPin) {
		m_pPropsetTunerPin->Release();
		m_pPropsetTunerPin = NULL;
	}

	return;
}

/////////////////////////////////////
//
// IBdaSpecials APIs
//
/////////////////////////////////////

// InitializeHook
/////////////////////////////////////
const HRESULT CTBSSpecials::InitializeHook(void)
{
	if (m_pTunerDevice == NULL) {
		return E_POINTER;
	}

	HRESULT hr;
	if (m_pPropsetTunerPin == NULL) {
		IEnumPins* pPinEnum = NULL;

		m_pTunerDevice->EnumPins(&pPinEnum);
		if (pPinEnum)
		{
			IPin* pPin=NULL;
			while (!(m_pPropsetTunerPin) && SUCCEEDED(pPinEnum->Next(1, &pPin, NULL)))
			{
				PIN_DIRECTION dir;
				if (SUCCEEDED(pPin->QueryDirection(&dir)))
				{						
					hr = pPin->QueryInterface(IID_IKsPropertySet, (void **) &m_pPropsetTunerPin);
				}
				pPin->Release();
			}
			pPinEnum->Release();
		}
		
		DWORD TypeSupport=0;
		if ((hr = m_pPropsetTunerPin->QuerySupported(KSPROPSETID_BdaTunerExtensionProperties,
			KSPROPERTY_BDA_DISEQC_MESSAGE, 
			&TypeSupport)) != S_OK) {
				return E_NOINTERFACE;
		}
	}

	hr = SetLNBPower(TRUE);
	if (hr != S_OK) {
		OutputDebug(L"SetLNBPower failed.\n");
	}

	return S_OK;
}

const HRESULT CTBSSpecials::Set22KHz(bool bActive)
{
	return Set22KHz(bActive ? (long)1 : (long)0);
}

const HRESULT CTBSSpecials::Set22KHz(long nTone)
{
	DWORD bytesReturned;
	DISEQC_MESSAGE_PARAMS DiSEqCRequest;
	ZeroMemory(&DiSEqCRequest, sizeof(DiSEqCRequest));
	DiSEqCRequest.tbscmd_mode = TBSDVBSCMD_22KTONEDATA;    
	DiSEqCRequest.Tone_Data_Burst = Value_Burst_OFF;
	DiSEqCRequest.HZ_22K = !!nTone ? HZ_22K_ON : HZ_22K_OFF;

	HRESULT hr;
	hr = m_pPropsetTunerPin->Get(KSPROPSETID_BdaTunerExtensionProperties,
		KSPROPERTY_BDA_DISEQC_MESSAGE, 
		&DiSEqCRequest,sizeof(DISEQC_MESSAGE_PARAMS),
		&DiSEqCRequest, sizeof(DISEQC_MESSAGE_PARAMS),
		&bytesReturned);

	return hr;

}

const HRESULT CTBSSpecials::FinalizeHook(void)
{

	HRESULT hr = SetLNBPower(FALSE);
	if (hr != S_OK) {
		OutputDebug(L"SetLNBPower failed.\n");
	}

	m_hMySelf = NULL;
	if (m_pTunerDevice) {
		m_pTunerDevice.Release();
		m_pTunerDevice = NULL; 
	}
	if (m_pPropsetTunerPin) {
		m_pPropsetTunerPin->Release();
		m_pPropsetTunerPin = NULL;
	}

	return S_OK;
}

const HRESULT CTBSSpecials::GetSignalState(int *pnStrength, int *pnQuality, int *pnLock)
{
	return E_NOINTERFACE;
}

const HRESULT CTBSSpecials::LockChannel(BYTE bySatellite, BOOL bHorizontal, unsigned long ulFrequency, BOOL bDvbS2)
{
	return E_NOINTERFACE;
}

const HRESULT CTBSSpecials::LockChannel(const TuningParam *pTuningParm)
{
	return E_NOINTERFACE;
}

const HRESULT CTBSSpecials::SetLNBPower(bool bActive)
{
	DWORD bytesReturned;
	DISEQC_MESSAGE_PARAMS DiSEqCRequest;
	ZeroMemory(&DiSEqCRequest, sizeof(DiSEqCRequest));
	DiSEqCRequest.tbscmd_mode = TBSDVBSCMD_LNBPOWER;
	DiSEqCRequest.b_LNBPower = bActive? LNB_POWER_ON : LNB_POWER_OFF;

	HRESULT hr;
	hr = m_pPropsetTunerPin->Get(KSPROPSETID_BdaTunerExtensionProperties,
		KSPROPERTY_BDA_DISEQC_MESSAGE,
		&DiSEqCRequest,sizeof(DISEQC_MESSAGE_PARAMS),
		&DiSEqCRequest, sizeof(DISEQC_MESSAGE_PARAMS),
		&bytesReturned);

	return hr;
}

const HRESULT CTBSSpecials::ReadIniFile(WCHAR *szIniFilePath)
{
	return E_NOINTERFACE;
}

const HRESULT CTBSSpecials::IsDecodingNeeded(BOOL *pbAns)
{
	if (pbAns)
		*pbAns = FALSE;

	return S_OK;
}

const HRESULT CTBSSpecials::Decode(BYTE *pBuf, DWORD dwSize)
{
	return E_NOINTERFACE;
}

const HRESULT CTBSSpecials::GetSignalStrength(float *fVal)
{
	return E_NOINTERFACE;
}

const HRESULT CTBSSpecials::PreTuneRequest(const TuningParam *pTuningParm, ITuneRequest *pITuneRequest)
{
	return E_NOINTERFACE;
}

const HRESULT CTBSSpecials::PostLockChannel(const TuningParam *pTuningParm)
{
	return E_NOINTERFACE;
}

void CTBSSpecials::Release(void)
{
	delete this;
}
