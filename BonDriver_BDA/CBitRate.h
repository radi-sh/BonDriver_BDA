#pragma once

#include <Windows.h>

// ビットレート計算用
class CBitRate {
private:
	DWORD Rate1sec;					// 1秒間のレート加算用 (bytes/sec)
	DWORD RateLast[5];				// 直近5秒間のレート (bytes/sec)
	DWORD DataCount;				// 直近5秒間のデータ個数 (0～5)
	double Rate;					// 平均ビットレート (Mibps)
	DWORD LastTick;					// 前回のTickCount値
	CRITICAL_SECTION csRate1Sec;	// nRate1sec 排他用
	CRITICAL_SECTION csRateLast;	// nRateLast 排他用

public:
	CBitRate(void);
	~CBitRate(void);
	void AddRate(DWORD Count);
	DWORD CheckRate(void);
	void Clear(void);
	double GetRate(void);
};
