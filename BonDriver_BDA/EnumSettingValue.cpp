#include <Windows.h>
#include<bdatypes.h>

#include "EnumSettingValue.h"

CIniFileAccess::Map EnumSettingValue::mapThreadPriority = {
	{ L"",                              THREAD_PRIORITY_ERROR_RETURN },
	{ L"THREAD_PRIORITY_IDLE",          THREAD_PRIORITY_IDLE },
	{ L"THREAD_PRIORITY_LOWEST",        THREAD_PRIORITY_LOWEST },
	{ L"THREAD_PRIORITY_BELOW_NORMAL",  THREAD_PRIORITY_BELOW_NORMAL },
	{ L"THREAD_PRIORITY_NORMAL",        THREAD_PRIORITY_NORMAL },
	{ L"THREAD_PRIORITY_ABOVE_NORMAL",  THREAD_PRIORITY_ABOVE_NORMAL },
	{ L"THREAD_PRIORITY_HIGHEST",       THREAD_PRIORITY_HIGHEST },
	{ L"THREAD_PRIORITY_TIME_CRITICAL", THREAD_PRIORITY_TIME_CRITICAL },
};

CIniFileAccess::Map EnumSettingValue::mapModulationType = {
	{ L"BDA_MOD_NOT_SET",          (int)ModulationType::BDA_MOD_NOT_SET },
	{ L"BDA_MOD_NOT_DEFINED",      (int)ModulationType::BDA_MOD_NOT_DEFINED },
	{ L"BDA_MOD_16QAM",            (int)ModulationType::BDA_MOD_16QAM },
	{ L"BDA_MOD_32QAM",            (int)ModulationType::BDA_MOD_32QAM },
	{ L"BDA_MOD_64QAM",            (int)ModulationType::BDA_MOD_64QAM },
	{ L"BDA_MOD_80QAM",            (int)ModulationType::BDA_MOD_80QAM },
	{ L"BDA_MOD_96QAM",            (int)ModulationType::BDA_MOD_96QAM },
	{ L"BDA_MOD_112QAM",           (int)ModulationType::BDA_MOD_112QAM },
	{ L"BDA_MOD_128QAM",           (int)ModulationType::BDA_MOD_128QAM },
	{ L"BDA_MOD_160QAM",           (int)ModulationType::BDA_MOD_160QAM },
	{ L"BDA_MOD_192QAM",           (int)ModulationType::BDA_MOD_192QAM },
	{ L"BDA_MOD_224QAM",           (int)ModulationType::BDA_MOD_224QAM },
	{ L"BDA_MOD_256QAM",           (int)ModulationType::BDA_MOD_256QAM },
	{ L"BDA_MOD_320QAM",           (int)ModulationType::BDA_MOD_320QAM },
	{ L"BDA_MOD_384QAM",           (int)ModulationType::BDA_MOD_384QAM },
	{ L"BDA_MOD_448QAM",           (int)ModulationType::BDA_MOD_448QAM },
	{ L"BDA_MOD_512QAM",           (int)ModulationType::BDA_MOD_512QAM },
	{ L"BDA_MOD_640QAM",           (int)ModulationType::BDA_MOD_640QAM },
	{ L"BDA_MOD_768QAM",           (int)ModulationType::BDA_MOD_768QAM },
	{ L"BDA_MOD_896QAM",           (int)ModulationType::BDA_MOD_896QAM },
	{ L"BDA_MOD_1024QAM",          (int)ModulationType::BDA_MOD_1024QAM },
	{ L"BDA_MOD_QPSK",             (int)ModulationType::BDA_MOD_QPSK },
	{ L"BDA_MOD_BPSK",             (int)ModulationType::BDA_MOD_BPSK },
	{ L"BDA_MOD_OQPSK",            (int)ModulationType::BDA_MOD_OQPSK },
	{ L"BDA_MOD_8VSB",             (int)ModulationType::BDA_MOD_8VSB },
	{ L"BDA_MOD_16VSB",            (int)ModulationType::BDA_MOD_16VSB },
	{ L"BDA_MOD_ANALOG_AMPLITUDE", (int)ModulationType::BDA_MOD_ANALOG_AMPLITUDE },
	{ L"BDA_MOD_ANALOG_FREQUENCY", (int)ModulationType::BDA_MOD_ANALOG_FREQUENCY },
	{ L"BDA_MOD_8PSK",             (int)ModulationType::BDA_MOD_8PSK },
	{ L"BDA_MOD_RF",               (int)ModulationType::BDA_MOD_RF },
	{ L"BDA_MOD_16APSK",           (int)ModulationType::BDA_MOD_16APSK },
	{ L"BDA_MOD_32APSK",           (int)ModulationType::BDA_MOD_32APSK },
	{ L"BDA_MOD_NBC_QPSK",         (int)ModulationType::BDA_MOD_NBC_QPSK },
	{ L"BDA_MOD_NBC_8PSK",         (int)ModulationType::BDA_MOD_NBC_8PSK },
	{ L"BDA_MOD_DIRECTV",          (int)ModulationType::BDA_MOD_DIRECTV },
	{ L"BDA_MOD_ISDB_T_TMCC",      (int)ModulationType::BDA_MOD_ISDB_T_TMCC },
	{ L"BDA_MOD_ISDB_S_TMCC",      (int)ModulationType::BDA_MOD_ISDB_S_TMCC },
};

CIniFileAccess::Map EnumSettingValue::mapFECMethod = {
	{ L"BDA_FEC_METHOD_NOT_SET",     (int)FECMethod::BDA_FEC_METHOD_NOT_SET },
	{ L"BDA_FEC_METHOD_NOT_DEFINED", (int)FECMethod::BDA_FEC_METHOD_NOT_DEFINED },
	{ L"BDA_FEC_VITERBI",            (int)FECMethod::BDA_FEC_VITERBI },
	{ L"BDA_FEC_RS_204_188",         (int)FECMethod::BDA_FEC_RS_204_188 },
	{ L"BDA_FEC_LDPC",               (int)FECMethod::BDA_FEC_LDPC },
	{ L"BDA_FEC_BCH",                (int)FECMethod::BDA_FEC_BCH },
	{ L"BDA_FEC_RS_147_130",         (int)FECMethod::BDA_FEC_RS_147_130 },
};

CIniFileAccess::Map EnumSettingValue::mapBinaryConvolutionCodeRate = {
	{ L"BDA_BCC_RATE_NOT_SET",     (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_NOT_SET },
	{ L"BDA_BCC_RATE_NOT_DEFINED", (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_NOT_DEFINED },
	{ L"BDA_BCC_RATE_1_2",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_1_2 },
	{ L"BDA_BCC_RATE_2_3",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_2_3 },
	{ L"BDA_BCC_RATE_3_4",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_3_4 },
	{ L"BDA_BCC_RATE_3_5",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_3_5 },
	{ L"BDA_BCC_RATE_4_5",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_4_5 },
	{ L"BDA_BCC_RATE_5_6",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_5_6 },
	{ L"BDA_BCC_RATE_5_11",        (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_5_11 },
	{ L"BDA_BCC_RATE_7_8",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_7_8 },
	{ L"BDA_BCC_RATE_1_4",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_1_4 },
	{ L"BDA_BCC_RATE_1_3",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_1_3 },
	{ L"BDA_BCC_RATE_2_5",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_2_5 },
	{ L"BDA_BCC_RATE_6_7",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_6_7 },
	{ L"BDA_BCC_RATE_8_9",         (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_8_9 },
	{ L"BDA_BCC_RATE_9_10",        (int)BinaryConvolutionCodeRate::BDA_BCC_RATE_9_10 },
};

CIniFileAccess::Map EnumSettingValue::mapTuningSpaceType = {
	{ L"DVB-S/DVB-S2",  (int)EnumSettingValue::TunerType::DVBS },
	{ L"DVB-S2",        (int)EnumSettingValue::TunerType::DVBS },
	{ L"DVB-S",         (int)EnumSettingValue::TunerType::DVBS },
	{ L"DVB-T",         (int)EnumSettingValue::TunerType::DVBT },
	{ L"DVB-C",         (int)EnumSettingValue::TunerType::DVBC },
	{ L"DVB-T2",        (int)EnumSettingValue::TunerType::DVBT2 },
	{ L"ISDB-S",        (int)EnumSettingValue::TunerType::ISDBS },
	{ L"ISDB-T",        (int)EnumSettingValue::TunerType::ISDBT },
	{ L"ISDB-C",        (int)EnumSettingValue::TunerType::ISDBC },
	{ L"ATSC",          (int)EnumSettingValue::TunerType::ATSC_Antenna },
	{ L"ATSC CABLE",    (int)EnumSettingValue::TunerType::ATSC_Cable },
	{ L"DIGITAL CABLE", (int)EnumSettingValue::TunerType::DigitalCable },
};

CIniFileAccess::Map EnumSettingValue::mapSpecifyTuningSpace = {
	{ L"AUTO",                     (int)EnumSettingValue::TuningSpace::Auto },
	{ L"DVBTUNINGSPACE",           (int)EnumSettingValue::TuningSpace::DVB },
	{ L"DVBSTUNINGSPACE",          (int)EnumSettingValue::TuningSpace::DVBS },
	{ L"ANALOGTVTUNINGSPACE",      (int)EnumSettingValue::TuningSpace::AnalogTV },
	{ L"ATSCTUNINGSPACE",	       (int)EnumSettingValue::TuningSpace::ATSC },
	{ L"DIGITALCABLETUNINGSPACE",  (int)EnumSettingValue::TuningSpace::DigitalCable },
};

CIniFileAccess::Map EnumSettingValue::mapSpecifyLocator = {
	{ L"AUTO",                (int)EnumSettingValue::Locator::Auto },
	{ L"DVBTLOCATOR",         (int)EnumSettingValue::Locator::DVBT },
	{ L"DVBTLOCATOR2",        (int)EnumSettingValue::Locator::DVBT2 },
	{ L"DVBSLOCATOR",         (int)EnumSettingValue::Locator::DVBS },
	{ L"DVBCLOCATOR",         (int)EnumSettingValue::Locator::DVBC },
	{ L"ISDBSLOCATOR",        (int)EnumSettingValue::Locator::ISDBS },
	{ L"ATSCLOCATOR",         (int)EnumSettingValue::Locator::ATSC },
	{ L"DIGITALCABLELOCATOR", (int)EnumSettingValue::Locator::DigitalCable },
};

CIniFileAccess::Map EnumSettingValue::mapSpecifyITuningSpaceNetworkType = {
	{ L"AUTO",                                       (int)EnumSettingValue::NetworkType::Auto },
	{ L"STATIC_DVB_TERRESTRIAL_TV_NETWORK_TYPE",     (int)EnumSettingValue::NetworkType::DVBT },
	{ L"STATIC_DVB_SATELLITE_TV_NETWORK_TYPE",       (int)EnumSettingValue::NetworkType::DVBS },
	{ L"STATIC_DVB_CABLE_TV_NETWORK_TYPE",           (int)EnumSettingValue::NetworkType::DVBC },
	{ L"STATIC_ISDB_TERRESTRIAL_TV_NETWORK_TYPE",    (int)EnumSettingValue::NetworkType::ISDBT },
	{ L"STATIC_ISDB_SATELLITE_TV_NETWORK_TYPE",      (int)EnumSettingValue::NetworkType::ISDBS },
	{ L"STATIC_ISDB_CABLE_TV_NETWORK_TYPE",          (int)EnumSettingValue::NetworkType::ISDBC },
	{ L"STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE",    (int)EnumSettingValue::NetworkType::ATSC },
	{ L"STATIC_DIGITAL_CABLE_NETWORK_TYPE",          (int)EnumSettingValue::NetworkType::DigitalCable },
	{ L"STATIC_BSKYB_TERRESTRIAL_TV_NETWORK_TYPE",   (int)EnumSettingValue::NetworkType::BSkyB },
	{ L"STATIC_DIRECT_TV_SATELLITE_TV_NETWORK_TYPE", (int)EnumSettingValue::NetworkType::DIRECTV },
	{ L"STATIC_ECHOSTAR_SATELLITE_TV_NETWORK_TYPE",  (int)EnumSettingValue::NetworkType::EchoStar },
};

CIniFileAccess::Map EnumSettingValue::mapSpecifyIDVBTuningSpaceSystemType = {
	{ L"AUTO",             (int)EnumSettingValue::DVBSystemType::Auto },
	{ L"DVB_CABLE",        (int)EnumSettingValue::DVBSystemType::DVBC },
	{ L"DVB_TERRESTRIAL",  (int)EnumSettingValue::DVBSystemType::DVBT },
	{ L"DVB_SATELLITE",    (int)EnumSettingValue::DVBSystemType::DVBS },
	{ L"ISDB_TERRESTRIAL", (int)EnumSettingValue::DVBSystemType::ISDBT },
	{ L"ISDB_SATELLITE",   (int)EnumSettingValue::DVBSystemType::ISDBS },
};

CIniFileAccess::Map EnumSettingValue::mapSpecifyIAnalogTVTuningSpaceInputType = {
	{ L"AUTO",              (int)EnumSettingValue::TunerInputType::Auto },
	{ L"TUNERINPUTCABLE",   (int)EnumSettingValue::TunerInputType::Cable },
	{ L"TUNERINPUTANTENNA", (int)EnumSettingValue::TunerInputType::Antenna },
};

CIniFileAccess::Map EnumSettingValue::mapNetworkProvider = {
	{ L"AUTO",                             (int)EnumSettingValue::NetworkProvider::Auto },
	{ L"MICROSOFT NETWORK PROVIDER",       (int)EnumSettingValue::NetworkProvider::Generic },
	{ L"MICROSOFT DVB-S NETWORK PROVIDER", (int)EnumSettingValue::NetworkProvider::DVBS },
	{ L"MICROSOFT DVB-T NETWORK PROVIDER", (int)EnumSettingValue::NetworkProvider::DVBT },
	{ L"MICROSOFT DVB-C NETWORK PROVIDER", (int)EnumSettingValue::NetworkProvider::DVBC },
	{ L"MICROSOFT ATSC NETWORK PROVIDER",  (int)EnumSettingValue::NetworkProvider::ATSC },
};

CIniFileAccess::Map EnumSettingValue::mapDefaultNetwork = {
	{ L"NONE",     (int)EnumSettingValue::DefaultNetwork::None },
	{ L"SPHD",     (int)EnumSettingValue::DefaultNetwork::SPHD },
	{ L"BS/CS110", (int)EnumSettingValue::DefaultNetwork::BSCS },
	{ L"BS",       (int)EnumSettingValue::DefaultNetwork::BSCS },
	{ L"CS110",    (int)EnumSettingValue::DefaultNetwork::BSCS },
	{ L"UHF/CATV", (int)EnumSettingValue::DefaultNetwork::UHF },
	{ L"UHF",      (int)EnumSettingValue::DefaultNetwork::UHF },
	{ L"CATV",     (int)EnumSettingValue::DefaultNetwork::UHF },
	{ L"DUAL",     (int)EnumSettingValue::DefaultNetwork::Dual },
};

CIniFileAccess::Map EnumSettingValue::mapSignalLevelCalcType = {
	{ L"SSSTRENGTH",      (int)EnumSettingValue::SignalLevelCalcType::SSStrength },
	{ L"SSQUALITY",       (int)EnumSettingValue::SignalLevelCalcType::SSQuality },
	{ L"SSMUL",           (int)EnumSettingValue::SignalLevelCalcType::SSMul },
	{ L"SSADD",           (int)EnumSettingValue::SignalLevelCalcType::SSAdd },
	{ L"SSFORMULA",       (int)EnumSettingValue::SignalLevelCalcType::SSFormula },
	{ L"TUNERSTRENGTH",   (int)EnumSettingValue::SignalLevelCalcType::TunerStrength },
	{ L"TUNERQUALITY",    (int)EnumSettingValue::SignalLevelCalcType::TunerQuality },
	{ L"TUNERMUL",        (int)EnumSettingValue::SignalLevelCalcType::TunerMul },
	{ L"TUNERADD",        (int)EnumSettingValue::SignalLevelCalcType::TunerAdd },
	{ L"TUNERFORMULA",    (int)EnumSettingValue::SignalLevelCalcType::TunerFormula },
	{ L"DEMODSSSTRENGTH", (int)EnumSettingValue::SignalLevelCalcType::DemodSSStrength },
	{ L"DEMODSSQUALITY",  (int)EnumSettingValue::SignalLevelCalcType::DemodSSQuality },
	{ L"DEMODSSMUL",      (int)EnumSettingValue::SignalLevelCalcType::DemodSSMul },
	{ L"DEMODSSADD",      (int)EnumSettingValue::SignalLevelCalcType::DemodSSAdd },
	{ L"DEMODSSFORMULA",  (int)EnumSettingValue::SignalLevelCalcType::DemodSSFormula },
	{ L"BITRATE",         (int)EnumSettingValue::SignalLevelCalcType::BR },
};

CIniFileAccess::Map EnumSettingValue::mapSignalLockedJudgeType = {
	{ L"ALWAYS",        (int)EnumSettingValue::SignalLockedJudgeType::Always },
	{ L"SSLOCKED",      (int)EnumSettingValue::SignalLockedJudgeType::SS },
	{ L"TUNERSTRENGTH", (int)EnumSettingValue::SignalLockedJudgeType::Tuner },
	{ L"DEMODSSLOCKED", (int)EnumSettingValue::SignalLockedJudgeType::DemodSS },
};

CIniFileAccess::Map EnumSettingValue::mapDiSEqC = {
	{ L"",       (int)LNB_Source::BDA_LNB_SOURCE_NOT_SET },
	{ L"PORT-A", (int)LNB_Source::BDA_LNB_SOURCE_A },
	{ L"PORT-B", (int)LNB_Source::BDA_LNB_SOURCE_B },
	{ L"PORT-C", (int)LNB_Source::BDA_LNB_SOURCE_C },
	{ L"PORT-D", (int)LNB_Source::BDA_LNB_SOURCE_D },
};

CIniFileAccess::Map EnumSettingValue::mapTSMFMode = {
	{ L"OFF",      (int)EnumSettingValue::TSMFMode::Off },
	{ L"TSID",     (int)EnumSettingValue::TSMFMode::TSID },
	{ L"RELATIVE", (int)EnumSettingValue::TSMFMode::Relarive },
};
