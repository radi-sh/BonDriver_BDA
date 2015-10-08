#pragma once

#include <tuner.h>

#include "LockChannel.h"
#include "IBdaSpecials.h"

class IBdaSpecials2a : public IBdaSpecials
{
public:
	virtual const HRESULT Set22KHz(long nTone) = 0;
	virtual const HRESULT LockChannel(const TuningParam *pTuningParm) = 0;
	virtual const HRESULT ReadIniFile(WCHAR *szIniFilePath) = 0;
	virtual const HRESULT IsDecodingNeeded(BOOL *pbAns) = 0;
	virtual const HRESULT Decode(BYTE *pBuf, DWORD dwSize) = 0;
	virtual const HRESULT GetSignalStrength(float *fVal) = 0;
	virtual const HRESULT PreTuneRequest(const TuningParam *pTuningParm, ITuneRequest *pITuneRequest) = 0;
	virtual const HRESULT PostLockChannel(const TuningParam *pTuningParm) = 0;

	virtual void Release(void) = 0;
};

#if defined _USRDLL && !defined BONDRIVER_EXPORTS
extern "C" __declspec(dllexport) HRESULT CheckAndInitTuner(IBaseFilter *pTunerDevice, const WCHAR *szDisplayName, const WCHAR *szFriendlyName, const WCHAR *szIniFilePath);
#else
extern "C" __declspec(dllimport) HRESULT CheckAndInitTuner(IBaseFilter *pTunerDevice, const WCHAR *szDisplayName, const WCHAR *szFriendlyName, const WCHAR *szIniFilePath);
#endif
