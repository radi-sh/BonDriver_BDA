//------------------------------------------------------------------------------
// File: DVBWorldSpecials.h
//------------------------------------------------------------------------------
#pragma once

#include <Windows.h>
#include <string>

#include <initguid.h>

#include "IBdaSpecials2.h"

DEFINE_GUID( GUID_TUNER_S_LOCK,
0x8bed860a, 0xa7b4, 0x4e90, 0x9d, 0xf4, 0x13, 0x20, 0xc9, 0x49, 0x22, 0x61 ) ;

class CDVBWorldSpecials : public IBdaSpecials2b5
{
public:
	CDVBWorldSpecials(CComPtr<IBaseFilter> pTunerDevice);
	virtual ~CDVBWorldSpecials(void);

	const HRESULT InitializeHook(void);
	const HRESULT Set22KHz(bool bActive);
	const HRESULT FinalizeHook(void);
	const HRESULT LockChannel(BYTE bySatellite, BOOL bHorizontal, unsigned long ulFrequency, BOOL bDvbS2);

	const HRESULT Set22KHz(long nTone);
	const HRESULT LockChannel(const TuningParam *pTuningParam);

	virtual void Release(void);

	static HMODULE m_hMySelf;

private:
	HANDLE m_hTuner;
	CComPtr<IBaseFilter> m_pTunerDevice;

	////////////////////////////////////////////////////
	// Definitions for DVBWorld
	////////////////////////////////////////////////////
	typedef struct _Tuner_S_Param2 {
		GUID GUID_ID;				// = GUID_TUNER_S_LOCK
		unsigned long symbol_rate;	// Kbps
		unsigned long frequency;	// KHz
		unsigned long lnb;			// KHz
		int hv;						// 0=V, 1=H
		bool b22k;					// 0=off,1=on
		int diseqcPort;				// 1-4
		int FEC;					// BDA_BCC_RATE_x_x
		int Modulation;				// 1=QPSK,3=8PSK
		int Burst;					// 0=Undefined,1=Burst-A,2=Burst-B
	} Tuner_S_Param2;


	// polarization
	static constexpr int LINEAR_V = 0;
	static constexpr int LINEAR_H = 1;

	// DiSeqC
	static constexpr int DISEQC_PORT_A = 1;
	static constexpr int DISEQC_PORT_B = 2;
	static constexpr int DISEQC_PORT_C = 3;
	static constexpr int DISEQC_PORT_D = 4;

	// Modulation
	static constexpr int DW_MOD_DVBS1_QPSK = 1;
	static constexpr int DW_MOD_DVBS2_QPSK = 2;
	static constexpr int DW_MOD_DVBS2_8PSK = 3;

	// Burst
	static constexpr int DW_BURST_UNDEFINED = 0;
	static constexpr int DW_BURST_A = 1;
	static constexpr int DW_BURST_B = 2;

};
