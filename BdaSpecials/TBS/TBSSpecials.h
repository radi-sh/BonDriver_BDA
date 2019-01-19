//------------------------------------------------------------------------------
// File: TBSSpecials.h
//------------------------------------------------------------------------------
#pragma once

#include "IBdaSpecials2.h"

// GUID/Structure/constant for tuning

// this is defined in bda tuner/demod driver source (splmedia.h)
const GUID KSPROPSETID_BdaTunerExtensionProperties =
{0xfaa8f3e5, 0x31d4, 0x4e41, {0x88, 0xef, 0xd9, 0xeb, 0x71, 0x6f, 0x6e, 0xc9}};

class CTBSSpecials : public IBdaSpecials2b1
{
public:
	CTBSSpecials(HMODULE hMySelf, CComPtr<IBaseFilter> pTunerDevice);
	virtual ~CTBSSpecials(void);

	const HRESULT InitializeHook(void);
	const HRESULT Set22KHz(bool bActive);
	const HRESULT FinalizeHook(void);

	const HRESULT GetSignalState(int *pnStrength, int *pnQuality, int *pnLock);
	const HRESULT LockChannel(BYTE bySatellite, BOOL bHorizontal, unsigned long ulFrequency, BOOL bDvbS2);
	const HRESULT SetLNBPower(bool bActive);

	const HRESULT Set22KHz(long nTone);
	const HRESULT LockChannel(const TuningParam *pTuningParam);
	const HRESULT ReadIniFile(const WCHAR *szIniFilePath);
	const HRESULT IsDecodingNeeded(BOOL *pbAns);
	const HRESULT Decode(BYTE *pBuf, DWORD dwSize);
	const HRESULT GetSignalStrength(float *fVal);
	const HRESULT PreLockChannel(TuningParam *pTuningParam);
	const HRESULT PreTuneRequest(const TuningParam *pTuningParam, ITuneRequest *pITuneRequest);
 	const HRESULT PostTuneRequest(const TuningParam *pTuningParam);
	const HRESULT PostLockChannel(const TuningParam *pTuningParam);

	virtual void Release(void);

private:
	HMODULE m_hMySelf;
	CComPtr<IKsPropertySet> m_pPropsetTunerPin;
	CComPtr<IBaseFilter> m_pTunerDevice;

	////////////////////////////////////////////////////
	// Definitions for TBS
	////////////////////////////////////////////////////
	static constexpr UCHAR HZ_22K_OFF = 0;
	static constexpr UCHAR HZ_22K_ON = 1;
	static constexpr UCHAR LNB_POWER_OFF = 0;
	static constexpr UCHAR LNB_POWER_ON = 1;

	static constexpr UCHAR Value_Data_Burst_ON = 1;
	static constexpr UCHAR Value_Tone_Burst_ON = 0;
	static constexpr UCHAR Value_Burst_OFF = 2;

	static constexpr BYTE DISEQC_TX_BUFFER_SIZE = 150; // 3 bytes per message * 50 messages
	static constexpr BYTE DISEQC_RX_BUFFER_SIZE = 8;   // reply fifo size, hardware limitation

	typedef enum _TBSDVBSExtensionPropertiesCMDMode {
		TBSDVBSCMD_LNBPOWER=0x00,
		TBSDVBSCMD_MOTOR=0x01,
		TBSDVBSCMD_22KTONEDATA=0x02,
		TBSDVBSCMD_DISEQC=0x03
	} TBSDVBSExtensionPropertiesCMDMode;

	// this is defined in bda tuner/demod driver source (splmedia.h)
	typedef enum {
		KSPROPERTY_BDA_DISEQC_MESSAGE = 0,  //Custom property for Diseqc messaging
		KSPROPERTY_BDA_DISEQC_INIT,         //Custom property for Intializing Diseqc.
		KSPROPERTY_BDA_SCAN_FREQ,           //Not supported 
		KSPROPERTY_BDA_CHANNEL_CHANGE,      //Custom property for changing channel
		KSPROPERTY_BDA_DEMOD_INFO,          //Custom property for returning demod FW state and version
		KSPROPERTY_BDA_EFFECTIVE_FREQ,      //Not supported 
		KSPROPERTY_BDA_SIGNAL_STATUS,       //Custom property for returning signal quality, strength, BER and other attributes
		KSPROPERTY_BDA_LOCK_STATUS,         //Custom property for returning demod lock indicators 
		KSPROPERTY_BDA_ERROR_CONTROL,       //Custom property for controlling error correction and BER window
		KSPROPERTY_BDA_CHANNEL_INFO,        //Custom property for exposing the locked values of frequency,symbol rate etc after
                                     		//corrections and adjustments
		KSPROPERTY_BDA_NBC_PARAMS
	} KSPROPERTY_BDA_TUNER_EXTENSION;

	/*******************************************************************************************************/
	/* PHANTOM_LNB_BURST */
	/*******************************************************************************************************/
	typedef enum _PHANTOMLnbburst  {
		PHANTOM_LNB_BURST_MODULATED=1,                /* Modulated: Tone B               */
		PHANTOM_LNB_BURST_UNMODULATED,                /* Not modulated: Tone A           */
		PHANTOM_LNB_BURST_UNDEF=0                     /* undefined (results in an error) */
	}   PHANTOM_LNB_BURST;
	/*******************************************************************************************************/
	/* PHANTOM_RXMODE */
	/*******************************************************************************************************/
	typedef enum _PHANTOMRxMode  {
		PHANTOM_RXMODE_INTERROGATION=0,              /* Demod expects multiple devices attached */
		PHANTOM_RXMODE_QUICKREPLY=1,                 /* demod expects 1 rx (rx is suspended after 1st rx received) */
		PHANTOM_RXMODE_NOREPLY=2                     /* demod expects to receive no Rx message(s) */
	}   PHANTOM_RXMODE;


	// DVB-S/S2 DiSEqC message parameters
	typedef struct _DISEQC_MESSAGE_PARAMS
	{
		UCHAR      uc_diseqc_send_message[DISEQC_TX_BUFFER_SIZE+1];
		UCHAR      uc_diseqc_send_message_length;
		UCHAR      uc_diseqc_receive_message[DISEQC_RX_BUFFER_SIZE+1];
		UCHAR      uc_diseqc_receive_message_length;    

		PHANTOM_LNB_BURST burst_tone;       //Burst tone at last-message: (modulated = ToneB; Un-modulated = ToneA). 
		PHANTOM_RXMODE    receive_mode;     //Reply mode: interrogation/no reply/quick reply.
		TBSDVBSExtensionPropertiesCMDMode tbscmd_mode;

		UCHAR           HZ_22K;                 // HZ_22K_OFF | HZ_22K_ON
		UCHAR           Tone_Data_Burst;        // Data_Burst_ON | Tone_Burst_ON |Tone_Data_Disable
		UCHAR      uc_parity_errors;        //Parity errors:  0 indicates no errors; binary 1 indicates an error.
		UCHAR      uc_reply_errors;         //1 in bit i indicates error in byte i. 
		BOOL       b_last_message;          //Indicates if current message is the last message (TRUE means last message).
		BOOL       b_LNBPower;//liuzheng added for lnb power static

	} DISEQC_MESSAGE_PARAMS, *PDISEQC_MESSAGE_PARAMS;

};
