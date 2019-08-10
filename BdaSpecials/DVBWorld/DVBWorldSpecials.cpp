//------------------------------------------------------------------------------
// File: DVBWorldSPecials.cpp
//   Implementation of CDVBWorldSpecials class
//------------------------------------------------------------------------------

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "common.h"

#include "DVBWorldSpecials.h"

#include <Windows.h>
#include <string>

#include <dshow.h>

#include <ks.h>
#pragma warning (push)
#pragma warning (disable: 4091)
#include <ksmedia.h>
#pragma warning (pop)
#include <bdatypes.h>
#define __STREAMS__
#include <ksproxy.h>

#pragma comment( lib, "ksproxy.lib" )

FILE *g_fpLog = NULL;

// Module handle (global)
/////////////////////////////////////////////

HMODULE CDVBWorldSpecials::m_hMySelf = NULL;

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
		CDVBWorldSpecials::m_hMySelf = hModule;
		break;

	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

// CreateBdaSpecials(void) method
/////////////////////////////////////////////
__declspec(dllexport) IBdaSpecials* CreateBdaSpecials(CComPtr<IBaseFilter> pTunerDevice)
{
	return new CDVBWorldSpecials(pTunerDevice);
}

// Constructor
/////////////////////////////////////

CDVBWorldSpecials::CDVBWorldSpecials(CComPtr<IBaseFilter> pTunerDevice)
	: m_pTunerDevice(pTunerDevice), m_hTuner(NULL)
{
	if (m_pTunerDevice == NULL) {
		return;
	}

	CComQIPtr<IKsObject> pIKsObject(m_pTunerDevice);
	if (!pIKsObject) {
		return;
	}
	m_hTuner = pIKsObject->KsGetObjectHandle();

	return;
}

// Destructor
/////////////////////////////////////

CDVBWorldSpecials::~CDVBWorldSpecials()
{
	m_hTuner = NULL;
	return;
}

/////////////////////////////////////
//
// IBdaSpecials APIs
//
/////////////////////////////////////

// InitializeHook
/////////////////////////////////////
const HRESULT CDVBWorldSpecials::InitializeHook(void)
{
	if (m_pTunerDevice == NULL) {
		return E_POINTER;
	}

	if (!m_hTuner) {
		return E_NOINTERFACE;
	}

	return S_OK;
}

const HRESULT CDVBWorldSpecials::Set22KHz(bool bActive)
{
	return Set22KHz(bActive ? (long)1 : (long)0);
}

const HRESULT CDVBWorldSpecials::Set22KHz(long nTone)
{
	Tuner_S_Param2 sTuneParam ;
	ZeroMemory(&sTuneParam, sizeof(sTuneParam));

	sTuneParam.GUID_ID = GUID_TUNER_S_LOCK ;	
	sTuneParam.b22k = !!nTone;
	sTuneParam.Burst = DW_BURST_UNDEFINED;

	DWORD dRet ;
	HRESULT hr;
	hr = KsSynchronousDeviceControl(m_hTuner, IOCTL_KS_PROPERTY, PVOID(&sTuneParam), sizeof(sTuneParam), NULL, 0, &dRet);

	return hr;
}

const HRESULT CDVBWorldSpecials::FinalizeHook(void)
{
	m_hTuner = NULL;
	return S_OK;
}

const HRESULT CDVBWorldSpecials::LockChannel(BYTE bySatellite, BOOL bHorizontal, unsigned long ulFrequency, BOOL bDvbS2)
{
	Tuner_S_Param2 sTuneParam;
	ZeroMemory(&sTuneParam, sizeof(sTuneParam));

	sTuneParam.GUID_ID = GUID_TUNER_S_LOCK;
	sTuneParam.frequency = ulFrequency * 1000;
	sTuneParam.symbol_rate = bDvbS2 ? 23303 : 21096;
	sTuneParam.lnb = 11200000;
	if (bySatellite == 1) // JCSAT-4
		sTuneParam.b22k = true;
	else
		sTuneParam.b22k = false;
	sTuneParam.hv = bHorizontal ? LINEAR_H : LINEAR_V;
	sTuneParam.diseqcPort = DISEQC_PORT_A;
	sTuneParam.FEC = bDvbS2 ? BDA_BCC_RATE_3_5 : BDA_BCC_RATE_3_4;
	sTuneParam.Modulation = bDvbS2 ? DW_MOD_DVBS2_8PSK : DW_MOD_DVBS1_QPSK;
	sTuneParam.Burst = DW_BURST_UNDEFINED;

	DWORD dRet;
	HRESULT hr;
	hr = KsSynchronousDeviceControl(m_hTuner, IOCTL_KS_PROPERTY,
		PVOID(&sTuneParam), sizeof(sTuneParam), NULL, 0, &dRet);

	return hr;
}

const HRESULT CDVBWorldSpecials::LockChannel(const TuningParam* pTuningParam)
{
    Tuner_S_Param2 sTuneParam;
	ZeroMemory(&sTuneParam, sizeof(sTuneParam));

	sTuneParam.GUID_ID = GUID_TUNER_S_LOCK;
	sTuneParam.frequency = pTuningParam->Frequency;
	sTuneParam.symbol_rate = pTuningParam->Modulation.SymbolRate;
	switch (pTuningParam->Polarisation) {
	case BDA_POLARISATION_LINEAR_H:
		sTuneParam.hv = 1;
		break;
	case BDA_POLARISATION_LINEAR_V:
	default:
		sTuneParam.hv = 0;
		break;
	}
	if (pTuningParam->Antenna.LNBSwitch != -1) {
		sTuneParam.b22k = (pTuningParam->Antenna.LNBSwitch >= pTuningParam->Frequency);
		sTuneParam.lnb = sTuneParam.b22k ? pTuningParam->Antenna.HighOscillator : pTuningParam->Antenna.LowOscillator;
	}
	else {
		sTuneParam.lnb = pTuningParam->Antenna.HighOscillator;
		sTuneParam.b22k = !!pTuningParam->Antenna.Tone;
	}
	sTuneParam.Burst = DW_BURST_UNDEFINED;
	if (pTuningParam->Antenna.DiSEqC >= 1 && pTuningParam->Antenna.DiSEqC <= 4) {
		sTuneParam.diseqcPort = pTuningParam->Antenna.DiSEqC;
	} else {
		sTuneParam.diseqcPort = DISEQC_PORT_A;
	}
	sTuneParam.FEC = pTuningParam->Modulation.InnerFECRate;
	switch (pTuningParam->Modulation.Modulation) {
	case BDA_MOD_NBC_QPSK:
	case BDA_MOD_QPSK:
		sTuneParam.Modulation = DW_MOD_DVBS1_QPSK;
		break;
	case BDA_MOD_NBC_8PSK:
	case BDA_MOD_8PSK:
		sTuneParam.Modulation = DW_MOD_DVBS2_8PSK;
		break;
	default:
		if (sTuneParam.Modulation < 21) {
			sTuneParam.Modulation = DW_MOD_DVBS1_QPSK;
		} else if (sTuneParam.Modulation < 27) {
			sTuneParam.Modulation = DW_MOD_DVBS2_QPSK;
		} else {
			sTuneParam.Modulation = DW_MOD_DVBS2_8PSK;
		}
		break;
	}

	DWORD dRet;
	HRESULT hr;
	hr = KsSynchronousDeviceControl(m_hTuner, IOCTL_KS_PROPERTY, PVOID(&sTuneParam), sizeof(sTuneParam), NULL, 0, &dRet);

	return hr;
}

void CDVBWorldSpecials::Release(void)
{
	delete this;
}

