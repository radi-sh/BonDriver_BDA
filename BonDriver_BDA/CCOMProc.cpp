#include "CCOMProc.h"
#include "common.h"
#include "WaitWithMsg.h"

CCOMProc::CCOMProc(void)
{
	hThreadInitComp = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	hReqEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	hEndEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	hTerminateRequest = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	::InitializeCriticalSection(&csLock);
}

CCOMProc::~CCOMProc(void)
{
	TerminateThread();
	SAFE_CLOSE_HANDLE(hThreadInitComp);
	SAFE_CLOSE_HANDLE(hReqEvent);
	SAFE_CLOSE_HANDLE(hEndEvent);
	SAFE_CLOSE_HANDLE(hTerminateRequest);
	::DeleteCriticalSection(&csLock);
}

HANDLE CCOMProc::CreateThread(LPTHREAD_START_ROUTINE proc, LPVOID param, int priority)
{
	// スレッド起動
	HANDLE hThreadTemp = NULL;
	hThreadTemp = ::CreateThread(NULL, 0, proc, param, 0, NULL);
	if (hThreadTemp)
		return hThreadTemp;

	// スレッドプライオリティ
	if (hThread != NULL && priority != THREAD_PRIORITY_ERROR_RETURN) {
		::SetThreadPriority(hThread, priority);
	}

	HANDLE h[2] = {
		hThreadTemp,
		hThreadInitComp,
	};

	DWORD ret = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	switch (ret)
	{
	case WAIT_OBJECT_0 + 1:
		break;

	case WAIT_OBJECT_0:
	default:
		try {
			SAFE_CLOSE_HANDLE(hThreadTemp);
		}
		catch (...) {
		}
		return NULL;
	}

	hThread = hThreadTemp;
	return hThread;
}

void CCOMProc::TerminateThread(void)
{
	// スレッド終了
	if (hThread) {
		::SetEvent(hTerminateRequest);
		::WaitForSingleObject(hThread, INFINITE);
		SAFE_CLOSE_HANDLE(hThread);
	}
}

void CCOMProc::NotifyThreadStarted(void)
{
	::SetEvent(hThreadInitComp);
}

BOOL CCOMProc::RequestCOMReq(CCOMProc::COMReqArgs* args)
{
	if (hThread == NULL)
		return FALSE;

	BOOL ret = FALSE;

	::EnterCriticalSection(&csLock);
	if (args) {
		nRequest = args->nRequest;
		uParam = args->uParam;
	}
	::SetEvent(hReqEvent);
	HANDLE h[2] = {
		hEndEvent,
		hThread
	};
	DWORD dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = TRUE;
		if (args) {
			args->uRetVal = uRetVal;
		}
	}
	::LeaveCriticalSection(&csLock);

	return ret;
}

CCOMProc::enumCOMWaitStatus CCOMProc::WaitRequest(DWORD msec, CCOMProc::enumCOMRequest* req, CCOMProc::COMReqParm* param)
{
	HANDLE h[2] = {
		hTerminateRequest,
		hReqEvent
	};
	enumCOMWaitStatus ret = (enumCOMWaitStatus)WaitForMultipleObjectsWithMessageLoop(2, h, FALSE, msec);
	if (ret == enumCOMWaitStatus::eCOMWaitRequestEvent) {
		if (req)
			*req = nRequest;

		if (param)
			*param = uParam;
	}
	return ret;
}

void CCOMProc::NotifyComplete(CCOMProc::COMReqRetVal val)
{
	uRetVal = val;
	::SetEvent(hEndEvent);
}

BOOL CCOMProc::CheckTick(void)
{
	dwTick = ::GetTickCount();
	if (dwTick - dwTickLastCheck > 1000) {
		dwTickLastCheck = dwTick;
		return TRUE;
	}
	return FALSE;
}

void CCOMProc::ResetWatchDog(void)
{
	bSignalLockErr = FALSE;
	bBitRateErr = FALSE;
}

BOOL CCOMProc::CheckSignalLockErr(BOOL state, DWORD threshold)
{
	if (state) {
		//正常
		bSignalLockErr = FALSE;
	}
	else {
		// 異常
		if (!bSignalLockErr) {
			// 今回発生
			bSignalLockErr = TRUE;
			dwTickSignalLockErr = dwTick;
		}
		else {
			// 前回以前に発生していた
			if ((dwTick - dwTickSignalLockErr) > threshold) {
				// 設定時間以上経過している
				ResetWatchDog();
				return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL CCOMProc::CheckBitRateErr(BOOL state, DWORD threshold)
{
	if (state) {
		//正常
		bSignalLockErr = FALSE;
	}
	else {
		// 異常
		if (!bBitRateErr) {
			// 今回発生
			bBitRateErr = TRUE;
			dwTickBitRateErr = dwTick;
		}
		else {
			// 前回以前に発生していた
			if ((dwTick - dwTickBitRateErr) > threshold) {
				// 設定時間以上経過している
				ResetWatchDog();
				return TRUE;
			}
		}
	}
	return FALSE;
}

void CCOMProc::SetReLockChannel(void)
{
	bDoReLockChannel = TRUE;
	nReLockFailCount = 0;
}

void CCOMProc::ResetReLockChannel(void)
{
	bDoReLockChannel = FALSE;
	nReLockFailCount = 0;
}

BOOL CCOMProc::NeedReLockChannel(void)
{
	return bDoReLockChannel;
}

BOOL CCOMProc::CheckReLockFailCount(unsigned int threshold)
{
	return (++nReLockFailCount >= threshold);
}

void CCOMProc::SetReOpenTuner(DWORD space, DWORD channel)
{
	bDoReOpenTuner = TRUE;
	dwReOpenSpace = space;
	dwReOpenChannel = channel;
}

void CCOMProc::ResetReOpenTuner(void)
{
	bDoReOpenTuner = FALSE;
	ClearReOpenChannel();
}

BOOL CCOMProc::NeedReOpenTuner(void)
{
	return bDoReOpenTuner;
}

DWORD CCOMProc::GetReOpenSpace(void)
{
	return dwReOpenSpace;
}

DWORD CCOMProc::GetReOpenChannel(void)
{
	return dwReOpenChannel;
}

void CCOMProc::ClearReOpenChannel(void)
{
	dwReOpenSpace = CCOMProc::SPACE_INVALID;
	dwReOpenChannel = CCOMProc::CHANNEL_INVALID;
}

BOOL CCOMProc::CheckReOpenChannel(void)
{
	return (dwReOpenSpace != CCOMProc::SPACE_INVALID && dwReOpenChannel != CCOMProc::CHANNEL_INVALID);
}

