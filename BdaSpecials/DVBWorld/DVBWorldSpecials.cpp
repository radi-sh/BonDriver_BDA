//------------------------------------------------------------------------------
// File: DVBWorldSPecials.cpp
//   Implementation of CDVBWorldSpecials class
//------------------------------------------------------------------------------

#include <Windows.h>
#include <stdio.h>

#include <string>

#include "DVBWorldSpecials.h"

#include <iostream>
#include <dshow.h>

#include <ks.h>
#pragma warning (push)
#pragma warning (disable: 4091)
#include <ksmedia.h>
#pragma warning (pop)
#include <bdatypes.h>
#define __STREAMS__
#include <ksproxy.h>

#include "common.h"

#pragma comment( lib, "Strmiids.lib" )
#pragma comment( lib, "ksproxy.lib" )

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
	return new CDVBWorldSpecials(hMySelf, pTunerDevice);
}

DEFINE_GUID( IID_IKsObject,
0x423c13a2, 0x2070, 0x11d0, 0x9e, 0xf7, 0x00, 0xaa, 0x00, 0xa2, 0x16, 0xa1 ) ;
// Constructor
/////////////////////////////////////

CDVBWorldSpecials::CDVBWorldSpecials(HMODULE hMySelf, CComPtr<IBaseFilter> pTunerDevice)
: m_hMySelf(hMySelf), m_pTunerDevice(pTunerDevice), m_hTuner(NULL)
{
	return;
}

// Destructor
/////////////////////////////////////

CDVBWorldSpecials::~CDVBWorldSpecials()
{
	m_hMySelf = NULL;
	m_hTuner = NULL;
	if (m_pTunerDevice) {
		m_pTunerDevice.Release();
		m_pTunerDevice = NULL; 
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
const HRESULT CDVBWorldSpecials::InitializeHook(void)
{
	if (m_pTunerDevice == NULL) {
		return E_POINTER;
	}

	if (m_hTuner == NULL) {
		HRESULT hr;
		CComPtr<IKsObject> pIKsObject ;
		if ((hr = m_pTunerDevice->QueryInterface(IID_IKsObject, (LPVOID*)&pIKsObject)) != S_OK) {
			return E_NOINTERFACE;
		}
		if ((m_hTuner = pIKsObject->KsGetObjectHandle()) == NULL) {
			return E_NOINTERFACE;
		}
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
	m_hMySelf = NULL;
	m_hTuner = NULL;
	if (m_pTunerDevice) {
		m_pTunerDevice.Release();
		m_pTunerDevice = NULL; 
	}
	return S_OK;
}

const HRESULT CDVBWorldSpecials::GetSignalState(int *pnStrength, int *pnQuality, int *pnLock)
{
	return E_NOINTERFACE;
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

const HRESULT CDVBWorldSpecials::LockChannel(const TuningParam *pTuningParm)
{
    Tuner_S_Param2 sTuneParam;
	ZeroMemory(&sTuneParam, sizeof(sTuneParam));

	sTuneParam.GUID_ID = GUID_TUNER_S_LOCK;
	sTuneParam.frequency = pTuningParm->Frequency;
	sTuneParam.symbol_rate = pTuningParm->Modulation->SymbolRate;
	switch (pTuningParm->Polarisation) {
	case BDA_POLARISATION_LINEAR_H:
		sTuneParam.hv = 1;
		break;
	case BDA_POLARISATION_LINEAR_V:
	default:
		sTuneParam.hv = 0;
		break;
	}
	if (pTuningParm->Antenna->LNBSwitch != -1) {
		sTuneParam.b22k = (pTuningParm->Antenna->LNBSwitch >= pTuningParm->Frequency);
		sTuneParam.lnb = sTuneParam.b22k ? pTuningParm->Antenna->HighOscillator : pTuningParm->Antenna->LowOscillator;
	}
	else {
		sTuneParam.lnb = pTuningParm->Antenna->HighOscillator;
		sTuneParam.b22k = !!pTuningParm->Antenna->Tone;
	}
	sTuneParam.Burst = DW_BURST_UNDEFINED;
	if (pTuningParm->Antenna->DiSEqC >= 1 && pTuningParm->Antenna->DiSEqC <= 4) {
		sTuneParam.diseqcPort = pTuningParm->Antenna->DiSEqC;
	} else {
		sTuneParam.diseqcPort = DISEQC_PORT_A;
	}
	sTuneParam.FEC = pTuningParm->Modulation->InnerFECRate;
	switch (pTuningParm->Modulation->Modulation) {
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

const HRESULT CDVBWorldSpecials::SetLNBPower(bool bActive)
{
	return E_NOINTERFACE;
}

const HRESULT CDVBWorldSpecials::ReadIniFile(WCHAR *szIniFilePath)
{
	return E_NOINTERFACE;
}

const HRESULT CDVBWorldSpecials::IsDecodingNeeded(BOOL *pbAns)
{
	if (pbAns)
		*pbAns = FALSE;

	return S_OK;
}

const HRESULT CDVBWorldSpecials::Decode(BYTE *pBuf, DWORD dwSize)
{
	return E_NOINTERFACE;
}

const HRESULT CDVBWorldSpecials::GetSignalStrength(float *fVal)
{
	return E_NOINTERFACE;
}

const HRESULT CDVBWorldSpecials::PreTuneRequest(const TuningParam *pTuningParm, ITuneRequest *pITuneRequest)
{
	return E_NOINTERFACE;
}

const HRESULT CDVBWorldSpecials::PostLockChannel(const TuningParam *pTuningParm)
{
	return E_NOINTERFACE;
}

void CDVBWorldSpecials::Release(void)
{
	delete this;
}

