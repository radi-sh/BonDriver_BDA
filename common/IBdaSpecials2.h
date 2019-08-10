#pragma once

#include <tuner.h>

#include "LockChannel.h"
#include "IBdaSpecials.h"

class IBdaSpecials2b5 : public IBdaSpecials
{
public:
#pragma warning (push)
#pragma warning (disable: 4100)
	virtual const HRESULT Set22KHz(bool bActive)
	{
		return E_NOINTERFACE;
	};

	virtual const HRESULT FinalizeHook(void)
	{
		return S_OK;
	}

	virtual const HRESULT GetSignalState(int* pnStrength, int* pnQuality, int* pnLock)
	{
		return E_NOINTERFACE;
	};

	virtual const HRESULT LockChannel(BYTE bySatellite, BOOL bHorizontal, unsigned long ulFrequency, BOOL bDvbS2)
	{
		return E_NOINTERFACE;
	};

	virtual const HRESULT SetLNBPower(bool bActive)
	{
		return E_NOINTERFACE;
	};

	virtual const HRESULT Set22KHz(long nTone)
	{
		return E_NOINTERFACE;
	};

	virtual const HRESULT SetTSid(long TSID)
	{
		return E_NOINTERFACE;
	};

	virtual const HRESULT LockChannel(const TuningParam* pTuningParam)
	{
		return E_NOINTERFACE;
	};

	virtual const HRESULT ReadIniFile(const WCHAR* szIniFilePath)
	{
		return E_NOINTERFACE;
	};

	virtual const HRESULT IsDecodingNeeded(BOOL* pbAns) {
		if (pbAns)
			* pbAns = FALSE;

		return S_OK;
	};

	virtual const HRESULT Decode(BYTE* pBuf, DWORD dwSize) {
		return E_NOINTERFACE;
	};

	virtual const HRESULT GetSignalStrength(float* fVal)
	{
		return E_NOINTERFACE;
	};

	virtual const HRESULT PreLockChannel(TuningParam* pTuningParam)
	{
		return S_OK;
	};

	virtual const HRESULT PreTuneRequest(const TuningParam* pTuningParam, ITuneRequest* pITuneRequest)
	{
		return S_OK;
	};

	virtual const HRESULT PostTuneRequest(const TuningParam* pTuningParam)
	{
		return S_OK;
	};

	virtual const HRESULT PostLockChannel(const TuningParam* pTuningParam)
	{
		return S_OK;
	};
#pragma warning (pop)

	virtual void Release(void) = 0;
};

#if defined _USRDLL && !defined BONDRIVER_EXPORTS
extern "C" __declspec(dllexport) IBdaSpecials* CreateBdaSpecials2(CComPtr<IBaseFilter> pTunerDevice, CComPtr<IBaseFilter> pCaptureDevice,
	const WCHAR* szTunerDisplayName, const WCHAR* szTunerFriendlyName, const WCHAR* szCaptureDisplayName, const WCHAR* szCaptureFriendlyName);
extern "C" __declspec(dllexport) HRESULT CheckAndInitTuner(IBaseFilter *pTunerDevice, const WCHAR *szDisplayName, const WCHAR *szFriendlyName, const WCHAR *szIniFilePath);
extern "C" __declspec(dllexport) HRESULT CheckCapture(const WCHAR *szTunerDisplayName, const WCHAR *szTunerFriendlyName,
	const WCHAR *szCaptureDisplayName, const WCHAR *szCaptureFriendlyName, const WCHAR *szIniFilePath);
#else
extern "C" __declspec(dllimport) IBdaSpecials* CreateBdaSpecials2(CComPtr<IBaseFilter> pTunerDevice, CComPtr<IBaseFilter> pCaptureDevice,
	const WCHAR* szTunerDisplayName, const WCHAR* szTunerFriendlyName, const WCHAR* szCaptureDisplayName, const WCHAR* szCaptureFriendlyName);
extern "C" __declspec(dllimport) HRESULT CheckAndInitTuner(IBaseFilter *pTunerDevice, const WCHAR *szDisplayName, const WCHAR *szFriendlyName, const WCHAR *szIniFilePath);
extern "C" __declspec(dllimport) HRESULT CheckCapture(const WCHAR *szTunerDisplayName, const WCHAR *szTunerFriendlyName,
	const WCHAR *szCaptureDisplayName, const WCHAR *szCaptureFriendlyName, const WCHAR *szIniFilePath);
#endif
