#pragma once

#include <bdatypes.h>

// アンテナ設定
struct AntennaParam {
	long Oscillator;		// LNB周波数 (MHz)
	long Tone;				// トーン信号 0 .. off, 1.. on
	long DiSEqC;			// DiSeqC指定
	AntennaParam(void)
		: Oscillator(-1),
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
	const AntennaParam *Antenna;		// アンテナ設定データ
	const ModulationMethod *Modulation;	// 変調方式設定データ
	long ONID;							// オリジナルネットワークID
	long TSID;							// トランスポートストリームID
	long SID;							// サービスID
	TuningParam(void)
		: Frequency(-1),
		  Polarisation(BDA_POLARISATION_NOT_SET),
		  Antenna(NULL),
		  Modulation(NULL),
		  ONID(-1),
		  TSID(-1),
		  SID(-1)
	{};
};
