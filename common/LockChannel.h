#pragma once

#include <bdatypes.h>

// アンテナ設定
struct AntennaParam {
	long HighOscillator;	// High側LNB周波数 (MHz)
	long LowOscillator;		// Low側LNB周波数 (MHz)
	long LNBSwitch;			// LNB切替周波数 (MHz)
	long Tone;				// トーン信号 0 .. off, 1.. on
	long DiSEqC;			// DiSeqC指定
	AntennaParam(void)
		: HighOscillator(-1),
		  LowOscillator(-1),
		  LNBSwitch(-1),
		  Tone(-1),
		  DiSEqC(-1)
	{};
};

// 変調方式設定データ
struct ModulationMethod {
	ModulationType Modulation;				// 変調タイプ
	FECMethod InnerFEC;						// 内部前方誤り訂正タイプ
	BinaryConvolutionCodeRate InnerFECRate;	// 内部FECレート
	FECMethod OuterFEC;						// 外部前方誤り訂正タイプ
	BinaryConvolutionCodeRate OuterFECRate;	// 外部FECレート
	long SymbolRate;						// シンボルレート
	long BandWidth;							// 帯域幅(MHz)
	ModulationMethod(void)
		: Modulation(BDA_MOD_NOT_SET),
		  InnerFEC(BDA_FEC_METHOD_NOT_SET),
		  InnerFECRate(BDA_BCC_RATE_NOT_SET),
		  OuterFEC(BDA_FEC_METHOD_NOT_SET),
		  OuterFECRate(BDA_BCC_RATE_NOT_SET),
		  SymbolRate(-1),
		  BandWidth(-1)
	{};
};

// LockChannel()で使用するチューニングパラメータ
struct TuningParam {
	long Frequency;						// 周波数(MHz)
	Polarisation Polarisation;			// 信号の偏波
	AntennaParam Antenna;				// アンテナ設定データ
	ModulationMethod Modulation;		// 変調方式設定データ
	union {
		long ONID;						// オリジナルネットワークID
		long PhysicalChannel;			// ATSC / Digital Cable用
	};
	union {
		long TSID;						// トランスポートストリームID
		long Channel;					// ATSC / Digital Cable用
	};
	union {
		long SID;						// サービスID
		long MinorChannel;				// ATSC / Digital Cable用
	};
	long MajorChannel;					// Digital Cable用
	long SourceID;						// Digital Cable用
	DWORD IniSpaceID;					// iniファイルで読込まれたチューニングスペース番号
	DWORD IniChannelID;					// iniファイルで読込まれたチャンネル番号
	TuningParam(void)
		: Frequency(-1),
		  Polarisation(BDA_POLARISATION_NOT_SET),
		  ONID(-1),
		  TSID(-1),
		  SID(-1),
		  MajorChannel(-1),
		  SourceID(-1),
		  IniSpaceID(0xFFFFFFFFL),
		  IniChannelID(0xFFFFFFFFL)
	{};
};
