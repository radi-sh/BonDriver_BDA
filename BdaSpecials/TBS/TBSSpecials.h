//------------------------------------------------------------------------------
// File: TBSSpecials.h
//------------------------------------------------------------------------------
#pragma once

#include "IBdaSpecials2.h"

// GUID/Structure/constant for tuning

class CTBSSpecials : public IBdaSpecials2b5
{
public:
	CTBSSpecials(CComPtr<IBaseFilter> pTunerDevice);
	virtual ~CTBSSpecials(void);

	const HRESULT InitializeHook(void);
	const HRESULT Set22KHz(bool bActive);
	const HRESULT FinalizeHook(void);
	const HRESULT SetLNBPower(bool bActive);

	const HRESULT Set22KHz(long nTone);
	const HRESULT SetTSid(long TSID); //add 20190715 by Davin zhang TBS
	const HRESULT ReadIniFile(const WCHAR* szIniFilePath);

	virtual void Release(void);

	static HMODULE m_hMySelf;

private:
	CComPtr<IKsPropertySet> m_pPropsetTunerPin;
	CComPtr<IBaseFilter> m_pTunerDevice;
	BOOL m_bLNBPowerON;									// 固有DllでLNB Power ONを行う
	BOOL m_bSet22KHz;									// 固有Dllでトーン制御を行う
	BOOL m_bSetTSID;									// 固有DllでTSIDをセットする

	////////////////////////////////////////////////////
	// Definitions for TBS
	////////////////////////////////////////////////////

	static constexpr BYTE DISEQC_TX_BUFFER_SIZE = 150;	// 3 bytes per message * 50 messages
	static constexpr BYTE DISEQC_RX_BUFFER_SIZE = 8;	// reply fifo size, hardware limitation

	// this is defined in bda tuner/demod driver source (splmedia.h)
	static constexpr GUID KSPROPSETID_BdaTunerExtensionProperties = { 0xfaa8f3e5, 0x31d4, 0x4e41, {0x88, 0xef, 0xd9, 0xeb, 0x71, 0x6f, 0x6e, 0xc9} };

	// this is defined in bda tuner/demod driver source (splmedia.h)
	enum KSPROPERTY_BDA_TUNER_EXTENSION : ULONG {
		KSPROPERTY_BDA_DISEQC_MESSAGE = 0,  // Custom property for Diseqc messaging
		KSPROPERTY_BDA_DISEQC_INIT,         // Custom property for Intializing Diseqc.
		KSPROPERTY_BDA_SCAN_FREQ,           // Not supported 
		KSPROPERTY_BDA_CHANNEL_CHANGE,      // Custom property for changing channel
		KSPROPERTY_BDA_DEMOD_INFO,          // Custom property for returning demod FW state and version
		KSPROPERTY_BDA_EFFECTIVE_FREQ,      // Not supported 
		KSPROPERTY_BDA_SIGNAL_STATUS,       // Custom property for returning signal quality, strength, BER and other attributes
		KSPROPERTY_BDA_LOCK_STATUS,         // Custom property for returning demod lock indicators 
		KSPROPERTY_BDA_ERROR_CONTROL,       // Custom property for controlling error correction and BER window
		KSPROPERTY_BDA_CHANNEL_INFO,        // Custom property for exposing the locked values of frequency,symbol rate etc after corrections and adjustments
		KSPROPERTY_BDA_NBC_PARAMS,
		KSPROPERTY_BDA_SETTSID = 96,		// added 20190704 for ISDB-S/S3 for BonBDA drivers
	};

	/*******************************************************************************************************/
	/* PHANTOM_LNB_BURST */
	/*******************************************************************************************************/
	enum PHANTOM_LNB_BURST : ULONG {
		PHANTOM_LNB_BURST_UNDEF = 0,				// undefined (results in an error)
		PHANTOM_LNB_BURST_MODULATED = 2,			// Modulated: Tone B
		PHANTOM_LNB_BURST_UNMODULATED = 3,			// Not modulated: Tone A
	};
	/*******************************************************************************************************/
	/* PHANTOM_RXMODE */
	/*******************************************************************************************************/
	enum PHANTOM_RXMODE : ULONG {
		PHANTOM_RXMODE_INTERROGATION = 0,			// Demod expects multiple devices attached
		PHANTOM_RXMODE_QUICKREPLY = 1,				// demod expects 1 rx (rx is suspended after 1st rx received)
		PHANTOM_RXMODE_NOREPLY = 2,					// demod expects to receive no Rx message(s)
	};

	enum TBSDVBSExtensionPropertiesCMDMode : ULONG {
		TBSDVBSCMD_LNBPOWER = 0x00,
		TBSDVBSCMD_MOTOR = 0x01,
		TBSDVBSCMD_22KTONEDATA = 0x02,
		TBSDVBSCMD_DISEQC = 0x03,
	};

	enum HZ_22K_TONE : BYTE {
		HZ_22K_OFF = 0,
		HZ_22K_ON = 1,
	};

	enum TONE_DATA_BURST : UCHAR {
		Tone_Burst_ON = 0,
		Data_Burst_ON = 1,
		Tone_Data_Disable = 2,
	};

	enum LNB_POWER : BOOL {
		LNB_POWER_OFF = FALSE,
		LNB_POWER_ON = TRUE,
	};

	// DVB-S/S2 DiSEqC message parameters
	struct DISEQC_MESSAGE_PARAMS
	{
		UCHAR uc_diseqc_send_message[DISEQC_TX_BUFFER_SIZE + 1];
		UCHAR uc_diseqc_send_message_length;
		UCHAR uc_diseqc_receive_message[DISEQC_RX_BUFFER_SIZE + 1];
		UCHAR uc_diseqc_receive_message_length;

		ULONG burst_tone;			// Burst tone at last-message: (modulated = ToneB; Un-modulated = ToneA). 
		ULONG receive_mode;			// Reply mode: interrogation/no reply/quick reply.
		ULONG tbscmd_mode;

		UCHAR HZ_22K;				// HZ_22K_OFF | HZ_22K_ON
		UCHAR Tone_Data_Burst;		// Data_Burst_ON | Tone_Burst_ON |Tone_Data_Disable
		UCHAR uc_parity_errors;		// Parity errors:  0 indicates no errors; binary 1 indicates an error.
		UCHAR uc_reply_errors;		// 1 in bit i indicates error in byte i. 
		BOOL b_last_message;		// Indicates if current message is the last message (TRUE means last message).
		BOOL b_LNBPower;			// liuzheng added for lnb power static
	};
};
