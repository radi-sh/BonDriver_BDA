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

#include "CIniFileAccess.h"
#include "DSFilterEnum.h"

FILE *g_fpLog = NULL;

// Module handle (global)
/////////////////////////////////////////////

HMODULE CTBSSpecials::m_hMySelf = NULL;

// DllMain
/////////////////////////////////////////////

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
#ifdef _DEBUG
		::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
		// モジュールハンドル保存
		CTBSSpecials::m_hMySelf = hModule;
		break;

	case DLL_PROCESS_DETACH:
		// デバッグログファイルのクローズ
		CloseDebugLog();
		break;
	}
	return TRUE;
}

// CreateBdaSpecials(void) method
/////////////////////////////////////////////
__declspec(dllexport) IBdaSpecials* CreateBdaSpecials(CComPtr<IBaseFilter> pTunerDevice)
{
	return new CTBSSpecials(pTunerDevice);
}

__declspec(dllexport) HRESULT CheckAndInitTuner(IBaseFilter* /*pTunerDevice*/, const WCHAR* /*szDisplayName*/, const WCHAR* /*szFriendlyName*/, const WCHAR* szIniFilePath)
{
	CIniFileAccess IniFileAccess(szIniFilePath);

	// DebugLogを記録するかどうか
	if (IniFileAccess.ReadKeyB(L"TBS", L"DebugLog", FALSE)) {
		// INIファイルのファイル名取得
		// DebugLogのファイル名取得
		SetDebugLog(common::GetModuleName(CTBSSpecials::m_hMySelf) + L"log");
	}

	return S_OK;
}

// Constructor
/////////////////////////////////////

CTBSSpecials::CTBSSpecials(CComPtr<IBaseFilter> pTunerDevice)
	: m_pTunerDevice(pTunerDevice),
	m_pPropsetTunerPin(NULL),
	m_bLNBPowerON(FALSE),
	m_bSet22KHz(FALSE),
	m_bSetTSID(FALSE)
{
	if (m_pTunerDevice) {
		CDSEnumPins DSEnumPins(m_pTunerDevice);
		while (1) {
			CComPtr<IPin> pPin;
			if (S_OK != DSEnumPins.getNextPin(&pPin, PIN_DIRECTION::PINDIR_INPUT)) {
				break;
			}
			CComQIPtr<IKsPropertySet> pPropsetTunerPin(pPin);
			if (pPropsetTunerPin) {
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
	if (!m_pTunerDevice) {
		OutputDebug(L"Invalid tuner divice pointer.\n");
		return E_POINTER;
	}

	if (!m_pPropsetTunerPin) {
		OutputDebug(L"Invalid IKsPropertySet pointer.\n");
		return E_POINTER;
	}
		
	if (m_bLNBPowerON) {
		SetLNBPower(TRUE);
	}

	return S_OK;
}

const HRESULT CTBSSpecials::Set22KHz(bool bActive)
{
	return Set22KHz(bActive ? (long)1 : (long)0);
}

const HRESULT CTBSSpecials::Set22KHz(long nTone)
{
	if (!m_bSet22KHz) {
		return E_NOINTERFACE;
	}

	if (!m_pPropsetTunerPin) {
		return E_POINTER;
	}

	HRESULT hr;
	DWORD TypeSupport = 0;
	if ((hr = m_pPropsetTunerPin->QuerySupported(KSPROPSETID_BdaTunerExtensionProperties, KSPROPERTY_BDA_DISEQC_MESSAGE, &TypeSupport)) != S_OK) {
		OutputDebug(L"Set22KHz: KSPROPERTY_BDA_DISEQC_MESSAGE is not supported.\n");
		m_bSet22KHz = FALSE;
		return E_NOINTERFACE;
	}

	DWORD bytesReturned;
	DISEQC_MESSAGE_PARAMS DiSEqCRequest;
	ZeroMemory(&DiSEqCRequest, sizeof(DiSEqCRequest));
	DiSEqCRequest.tbscmd_mode = TBSDVBSCMD_22KTONEDATA;    
	DiSEqCRequest.Tone_Data_Burst = Tone_Data_Disable;
	DiSEqCRequest.HZ_22K = !!nTone ? HZ_22K_ON : HZ_22K_OFF;

	hr = m_pPropsetTunerPin->Get(KSPROPSETID_BdaTunerExtensionProperties,
		KSPROPERTY_BDA_DISEQC_MESSAGE, 
		&DiSEqCRequest,sizeof(DISEQC_MESSAGE_PARAMS),
		&DiSEqCRequest, sizeof(DISEQC_MESSAGE_PARAMS),
		&bytesReturned);

	return hr;
}

const HRESULT CTBSSpecials::SetTSid(long TSID) // add 20190715 by Davin zhang TBS
{
	if (!m_bSetTSID) {
		return E_NOINTERFACE;
	}

	if (!m_pPropsetTunerPin) {
		return E_POINTER;
	}

	HRESULT hr;
	DWORD TypeSupport = 0;
	if ((hr = m_pPropsetTunerPin->QuerySupported(KSPROPSETID_BdaTunerExtensionProperties, KSPROPERTY_BDA_SETTSID, &TypeSupport)) != S_OK) {
		OutputDebug(L"SetTSID: KSPROPERTY_BDA_SETTSID is not supported.\n");
		m_bSetTSID = FALSE;
		return E_NOINTERFACE;
	}

	hr = m_pPropsetTunerPin->Set(KSPROPSETID_BdaTunerExtensionProperties,
		KSPROPERTY_BDA_SETTSID,
		&TSID, sizeof(long),
		&TSID, sizeof(long));

	return hr;
}

const HRESULT CTBSSpecials::ReadIniFile(const WCHAR* szIniFilePath)
{
	CIniFileAccess IniFileAccess(szIniFilePath);
	IniFileAccess.SetSectionName(L"TBS");

	// 固有DllでLNB Power ONを行う
	m_bLNBPowerON = IniFileAccess.ReadKeyB(L"LNBPowerON", m_bLNBPowerON);

	// 固有Dllでトーン制御を行う
	m_bSet22KHz = IniFileAccess.ReadKeyB(L"Set22KHz", m_bSet22KHz);

	// 固有DllでTSIDをセットする
	m_bSetTSID = IniFileAccess.ReadKeyB(L"SetTSID", m_bSetTSID);

	return S_OK;
}

const HRESULT CTBSSpecials::FinalizeHook(void)
{
	if (m_bLNBPowerON) {
		SetLNBPower(FALSE);
	}

	return S_OK;
}

const HRESULT CTBSSpecials::SetLNBPower(bool bActive)
{
	if (!m_bLNBPowerON) {
		return E_NOINTERFACE;
	}

	if (!m_pPropsetTunerPin) {
		return E_POINTER;
	}

	HRESULT hr;
	DWORD TypeSupport = 0;
	if ((hr = m_pPropsetTunerPin->QuerySupported(KSPROPSETID_BdaTunerExtensionProperties, KSPROPERTY_BDA_DISEQC_MESSAGE, &TypeSupport)) != S_OK) {
		OutputDebug(L"SetLNBPower: KSPROPERTY_BDA_DISEQC_MESSAGE is not supported.\n");
		m_bLNBPowerON = FALSE;
		return E_NOINTERFACE;
	}

	DWORD bytesReturned;
	DISEQC_MESSAGE_PARAMS DiSEqCRequest;
	ZeroMemory(&DiSEqCRequest, sizeof(DiSEqCRequest));
	DiSEqCRequest.tbscmd_mode = TBSDVBSCMD_LNBPOWER;
	DiSEqCRequest.b_LNBPower = bActive? LNB_POWER_ON : LNB_POWER_OFF;

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
