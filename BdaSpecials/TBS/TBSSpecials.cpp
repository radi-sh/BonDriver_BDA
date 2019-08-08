//------------------------------------------------------------------------------
// File: TBSSpecials.cpp
//   Implementation of CTBSSpecials class
//------------------------------------------------------------------------------

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "common.h"

#include "TBSSpecials.h"

#include <Windows.h>
#include <string>

#include <dshow.h>

#include "DSFilterEnum.h"

FILE *g_fpLog = NULL;

// Module handle (global)
/////////////////////////////////////////////

HMODULE hMySelf;

// DllMain
/////////////////////////////////////////////

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
#ifdef _DEBUG
		::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
		// モジュールハンドル保存
		hMySelf = hModule;
		break;

	case DLL_PROCESS_DETACH:
		break;
	}
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
: m_hMySelf(hMySelf), m_pTunerDevice(pTunerDevice)
{
	if (m_pTunerDevice) {
		CDSEnumPins DSEnumPins(m_pTunerDevice);
		while (1) {
			CComPtr<IPin> pPin;
			if (S_OK != DSEnumPins.getNextPin(&pPin, PIN_DIRECTION::PINDIR_INPUT)) {
				break;
			}
			CComQIPtr<IKsPropertySet> pPropsetTunerPin(pPin);
			if (!pPropsetTunerPin) {
				m_pPropsetTunerPin = pPropsetTunerPin;
				break;
			}
		}
	}
	return;
}

// Destructor
/////////////////////////////////////

CTBSSpecials::~CTBSSpecials()
{
	m_hMySelf = NULL;

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

	if (!m_pPropsetTunerPin) {
		return E_NOINTERFACE;
	}
		
	HRESULT hr;
	DWORD TypeSupport = 0;
	if ((hr = m_pPropsetTunerPin->QuerySupported(KSPROPSETID_BdaTunerExtensionProperties, KSPROPERTY_BDA_DISEQC_MESSAGE, &TypeSupport)) != S_OK) {
		return E_NOINTERFACE;
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

const HRESULT CTBSSpecials::SetTSid(long TSID) //add 20190715 by Davin zhang TBS
{
	DWORD TypeSupport = 0;
	HRESULT hr;

	hr = m_pPropsetTunerPin->QuerySupported(KSPROPSETID_BdaTunerExtensionProperties,
		KSPROPERTY_BDA_SETTSID, &TypeSupport);
	if FAILED(hr)
		return E_NOINTERFACE;

	hr = m_pPropsetTunerPin->Set(KSPROPSETID_BdaTunerExtensionProperties,
		KSPROPERTY_BDA_SETTSID,
		&TSID, sizeof(long),
		&TSID, sizeof(long));
	if FAILED(hr)
		return E_NOTIMPL;

	return S_OK;

}

const HRESULT CTBSSpecials::FinalizeHook(void)
{

	HRESULT hr = SetLNBPower(FALSE);
	if (hr != S_OK) {
		OutputDebug(L"SetLNBPower failed.\n");
	}

	m_hMySelf = NULL;

	return S_OK;
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

void CTBSSpecials::Release(void)
{
	delete this;
}
