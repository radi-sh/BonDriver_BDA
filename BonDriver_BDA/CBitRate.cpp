#include "CBitRate.h"

CBitRate::CBitRate(void)
{
	::InitializeCriticalSection(&csRate1Sec);
	::InitializeCriticalSection(&csRateLast);
	LastTick = ::GetTickCount();
};

CBitRate::~CBitRate(void)
{
	::DeleteCriticalSection(&csRateLast);
	::DeleteCriticalSection(&csRate1Sec);
}

void CBitRate::AddRate(DWORD Count)
{
	::EnterCriticalSection(&csRate1Sec);
	Rate1sec += Count;
	::LeaveCriticalSection(&csRate1Sec);
}

DWORD CBitRate::CheckRate(void)
{
	DWORD total = 0;
	DWORD Tick = ::GetTickCount();
	if (Tick - LastTick > 1000) {
		::EnterCriticalSection(&csRateLast);
		for (unsigned int i = (sizeof(RateLast) / sizeof(RateLast[0])) - 1; i > 0; i--) {
			RateLast[i] = RateLast[i - 1];
			total += RateLast[i];
		}
		::EnterCriticalSection(&csRate1Sec);
		RateLast[0] = Rate1sec;
		Rate1sec = 0;
		::LeaveCriticalSection(&csRate1Sec);
		total += RateLast[0];
		if (DataCount < 5)
			DataCount++;
		if (DataCount)
			Rate = ((double)total / (double)DataCount) / 131072.0;
		LastTick = Tick;
		::LeaveCriticalSection(&csRateLast);
	}
	DWORD remain = 1000 - (Tick - LastTick);
	return (remain > 1000) ? 1000 : remain;
}

void CBitRate::Clear(void)
{
	::EnterCriticalSection(&csRateLast);
	::EnterCriticalSection(&csRate1Sec);
	Rate1sec = 0;
	for (unsigned int i = 0; i < sizeof(RateLast) / sizeof(RateLast[0]); i++) {
		RateLast[i] = 0;
	}
	DataCount = 0;
	Rate = 0.0;
	LastTick = ::GetTickCount();
	::LeaveCriticalSection(&csRate1Sec);
	::LeaveCriticalSection(&csRateLast);
}

double CBitRate::GetRate(void)
{
	return Rate;
}
