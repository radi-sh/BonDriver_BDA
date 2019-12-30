#pragma once

#include <string>
#include <map>
#include "CIniFileAccess.h"

namespace EnumSettingValue {
	// SignalLevel 算出方法
	enum class SignalLevelCalcType : int {
		SSMin = 0,
		SSStrength = 0,			// RF Tuner NodeのIBDA_SignalStatisticsから取得したStrength値 ÷ StrengthCoefficient ＋ StrengthBias
		SSQuality = 1,			// RF Tuner NodeのIBDA_SignalStatisticsから取得したQuality値 ÷ QualityCoefficient ＋ QualityBias
		SSMul = 2,				// RF Tuner NodeのIBDA_SignalStatisticsから取得した(Strength値 ÷ StrengthCoefficient ＋ StrengthBias) × (Quality値 ÷ QualityCoefficient ＋ QualityBias)
		SSAdd = 3,				// RF Tuner NodeのIBDA_SignalStatisticsから取得した(Strength値 ÷ StrengthCoefficient ＋ StrengthBias) ＋ (Quality値 ÷ QualityCoefficient ＋ QualityBias)
		SSFormula = 9,			// RF Tuner NodeのIBDA_SignalStatisticsから取得したStrength/Quality値をSignalLevelCalcFormulaに設定したユーザー定義数式で算出
		SSMax = 9,
		TunerMin = 10,
		TunerStrength = 10,		// ITuner::get_SignalStrengthで取得したStrength値 ÷ StrengthCoefficient ＋ StrengthBias
		TunerQuality = 11,		// ITuner::get_SignalStrengthで取得したQuality値 ÷ QualityCoefficient ＋ QualityBias
		TunerMul = 12,			// ITuner::get_SignalStrengthで取得した(Strength値 ÷ StrengthCoefficient ＋ StrengthBias) × (Quality値 ÷ QualityCoefficient ＋ QualityBias)
		TunerAdd = 13,			// ITuner::get_SignalStrengthで取得した(Strength値 ÷ StrengthCoefficient ＋ StrengthBias) ＋ (Quality値 ÷ QualityCoefficient ＋ QualityBias)
		TunerFormula = 19,		// ITuner::get_SignalStrengthで取得したStrength/Quality値をSignalLevelCalcFormulaに設定したユーザー定義数式で算出
		TunerMax = 19,
		DemodSSMin = 20,
		DemodSSStrength = 20,	// Demodulator NodeのIBDA_SignalStatisticsから取得したStrength値 ÷ StrengthCoefficient ＋ StrengthBias
		DemodSSQuality = 21,	// Demodulator NodeのIBDA_SignalStatisticsから取得したQuality値 ÷ QualityCoefficient ＋ QualityBias
		DemodSSMul = 22,		// Demodulator NodeのIBDA_SignalStatisticsから取得した(Strength値 ÷ StrengthCoefficient ＋ StrengthBias) × (Quality値 ÷ QualityCoefficient ＋ QualityBias)
		DemodSSAdd = 23,		// Demodulator NodeのIBDA_SignalStatisticsから取得した(Strength値 ÷ StrengthCoefficient ＋ StrengthBias) ＋ (Quality値 ÷ QualityCoefficient ＋ QualityBias)
		DemodSSFormula = 29,	// Demodulator NodeのIBDA_SignalStatisticsから取得したStrength/Quality値をSignalLevelCalcFormulaに設定したユーザー定義数式で算出
		DemodSSMax = 29,
		BR = 100,				// ビットレート値(Mibps)
	};

	// チューニング状態の判断方法
	enum class SignalLockedJudgeType : int {
		Always = 0,				// 常にチューニングに成功している状態として判断する
		SS = 1,					// RF Tuner Node の IBDA_SignalStatistics::get_SignalLockedで取得した値で判断する
		Tuner = 2,				// ITuner::get_SignalStrengthで取得した値で判断する
		DemodSS = 3,			// Demodulator Node の IBDA_SignalStatistics::get_SignalLockedで取得した値で判断する
	};

	// チューナーの使用するTuningSpaceの種類
	enum class TunerType : int {
		None = -1,
		DVBS = 1,				// DBV-S/DVB-S2
		DVBT = 2,				// DVB-T
		DVBC = 3,				// DVB-C
		DVBT2 = 4,				// DVB-T2
		ISDBS = 11,				// ISDB-S
		ISDBT = 12,				// ISDB-T
		ISDBC = 13,				// ISDB-C
		ATSC_Antenna = 21,		// ATSC
		ATSC_Cable = 22,		// ATSC Cable
		DigitalCable = 23,		// Digital Cable
	};

	// 使用するTuningSpace オブジェクト
	enum class TuningSpace : int {
		Auto = -1,				// DVBSystemTypeの値によって自動選択
		DVB = 1,				// DVBTuningSpace
		DVBS = 2,				// DVBSTuningSpace
		AnalogTV = 21,			// AnalogTVTuningSpace
		ATSC = 22,				// ATSCTuningSpace
		DigitalCable = 23,		// DigitalCableTuningSpace
	};

	// 使用するLocator オブジェクト
	enum class Locator : int {
		Auto = -1,				// DVBSystemTypeの値によって自動選択
		DVBT = 1,				// DVBTLocator
		DVBT2 = 2,				// DVBTLocator2
		DVBS = 3,				// DVBSLocator
		DVBC = 4,				// DVBCLocator
		ISDBS = 11,				// ISDBSLocator
		ATSC = 21,				// ATSCLocator
		DigitalCable = 22,		// DigitalCableLocator
	};

	// ITuningSpaceに設定するNetworkType
	enum class NetworkType : int {
		Auto = -1,				// DVBSystemTypeの値によって自動選択
		DVBT = 1,				// STATIC_DVB_TERRESTRIAL_TV_NETWORK_TYPE
		DVBS = 2,				// STATIC_DVB_SATELLITE_TV_NETWORK_TYPE
		DVBC = 3,				// STATIC_DVB_CABLE_TV_NETWORK_TYPE
		ISDBT = 11,				// STATIC_ISDB_TERRESTRIAL_TV_NETWORK_TYPE
		ISDBS = 12,				// STATIC_ISDB_SATELLITE_TV_NETWORK_TYPE
		ISDBC = 13,				// STATIC_ISDB_CABLE_TV_NETWORK_TYPE
		ATSC = 21,				// STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE
		DigitalCable = 22,		// STATIC_DIGITAL_CABLE_NETWORK_TYPE
		BSkyB = 101,			// STATIC_BSKYB_TERRESTRIAL_TV_NETWORK_TYPE
		DIRECTV = 102,			// STATIC_DIRECT_TV_SATELLITE_TV_NETWORK_TYPE
		EchoStar = 103,			// STATIC_ECHOSTAR_SATELLITE_TV_NETWORK_TYPE
	};

	// IDVBTuningSpaceに設定するSystemType
	enum class DVBSystemType : int {
		Auto = -1,				// DVBSystemTypeの値によって自動選択
		DVBC = 0,				// DVB_Cable
		DVBT = 1,				// DVB_Terrestrial
		DVBS = 2,				// DVB_Satellite
		ISDBT = 3,				// ISDB_Terrestrial
		ISDBS = 4,				// ISDB_Satellite
	};

	// IAnalogTVTuningSpaceに設定するInputType
	enum class TunerInputType : int {
		Auto = -1,				// DVBSystemTypeの値によって自動選択
		Cable = 0,				// TunerInputCable
		Antenna = 1,			// TunerInputAntenna
	};

	// チューナーに使用するNetworkProvider 
	enum class NetworkProvider : int {
		Auto = 0,				// DVBSystemTypeの値によって自動選択
		Generic = 1,			// Microsoft Network Provider
		DVBS = 2,				// Microsoft DVB-S Network Provider
		DVBT = 3,				// Microsoft DVB-T Network Provider
		DVBC = 4,				// Microsoft DVB-C Network Provider
		ATSC = 5,				// Microsoft ATSC Network Provider
	};

	// 衛星受信パラメータ/変調方式パラメータのデフォルト値
	enum class DefaultNetwork : int {
		None = 0,				// 設定しない
		SPHD = 1,				// SPHD
		BSCS = 2,				// BS/CS110
		UHF = 3,				// UHF/CATV
		Dual = 4,				// Dual Mode (BS/CS110とUHF/CATV)
	};

	// TSMF処理設定
	enum class TSMFMode : int {
		Off = 0,				// 処理しない
		TSID = 1,				// TSID指定モード
		Relarive = 2,			// 相対番号指定モード
	};

	extern CIniFileAccess::Map mapThreadPriority;
	extern CIniFileAccess::Map mapModulationType;
	extern CIniFileAccess::Map mapFECMethod;
	extern CIniFileAccess::Map mapBinaryConvolutionCodeRate;
	extern CIniFileAccess::Map mapTuningSpaceType;
	extern CIniFileAccess::Map mapSpecifyTuningSpace;
	extern CIniFileAccess::Map mapSpecifyLocator;
	extern CIniFileAccess::Map mapSpecifyITuningSpaceNetworkType;
	extern CIniFileAccess::Map mapSpecifyIDVBTuningSpaceSystemType;
	extern CIniFileAccess::Map mapSpecifyIAnalogTVTuningSpaceInputType;
	extern CIniFileAccess::Map mapNetworkProvider;
	extern CIniFileAccess::Map mapDefaultNetwork;
	extern CIniFileAccess::Map mapSignalLevelCalcType;
	extern CIniFileAccess::Map mapSignalLockedJudgeType;
	extern CIniFileAccess::Map mapDiSEqC;
	extern CIniFileAccess::Map mapTSMFMode;
}
