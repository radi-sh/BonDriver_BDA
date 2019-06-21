#pragma once

#include <tuner.h>

#include "LockChannel.h"
#include "IBdaSpecials.h"

class IBdaSpecials2b3 : public IBdaSpecials
{
public:
	virtual const HRESULT Set22KHz(long nTone) = 0;
	virtual const HRESULT LockChannel(const TuningParam *pTuningParam) = 0;
	virtual const HRESULT ReadIniFile(const WCHAR *szIniFilePath) = 0;
	virtual const HRESULT IsDecodingNeeded(BOOL *pbAns) = 0;
	virtual const HRESULT Decode(BYTE *pBuf, DWORD dwSize) = 0;
	virtual const HRESULT GetSignalStrength(float *fVal) = 0;
	virtual const HRESULT PreLockChannel(TuningParam *pTuningParam) = 0;
	virtual const HRESULT PreTuneRequest(const TuningParam *pTuningParam, ITuneRequest *pITuneRequest) = 0;
	virtual const HRESULT PostTuneRequest(const TuningParam *pTuningParam) = 0;
	virtual const HRESULT PostLockChannel(const TuningParam *pTuningParam) = 0;

	virtual void Release(void) = 0;
};

#if defined _USRDLL && !defined BONDRIVER_EXPORTS
extern "C" __declspec(dllexport) IBdaSpecials* CreateBdaSpecials2(CComPtr<IBaseFilter> pTunerDevice, CComPtr<IBaseFilter> pCaptureDevice);
extern "C" __declspec(dllexport) HRESULT CheckAndInitTuner(IBaseFilter *pTunerDevice, const WCHAR *szDisplayName, const WCHAR *szFriendlyName, const WCHAR *szIniFilePath);
extern "C" __declspec(dllexport) HRESULT CheckCapture(const WCHAR *szTunerDisplayName, const WCHAR *szTunerFriendlyName,
	const WCHAR *szCaptureDisplayName, const WCHAR *szCaptureFriendlyName, const WCHAR *szIniFilePath);
#else
extern "C" __declspec(dllimport) IBdaSpecials* CreateBdaSpecials2(CComPtr<IBaseFilter> pTunerDevice, CComPtr<IBaseFilter> pCaptureDevice);
extern "C" __declspec(dllimport) HRESULT CheckAndInitTuner(IBaseFilter *pTunerDevice, const WCHAR *szDisplayName, const WCHAR *szFriendlyName, const WCHAR *szIniFilePath);
extern "C" __declspec(dllimport) HRESULT CheckCapture(const WCHAR *szTunerDisplayName, const WCHAR *szTunerFriendlyName,
	const WCHAR *szCaptureDisplayName, const WCHAR *szCaptureFriendlyName, const WCHAR *szIniFilePath);
#endif
