#include "CDecodeProc.h"
#include "common.h"
#include "WaitWithMsg.h"

CDecodeProc::CDecodeProc(void)
{
	hThreadInitComp = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	hTerminateRequest = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}
CDecodeProc::~CDecodeProc(void)
{
	TerminateThread();
	SAFE_CLOSE_HANDLE(hThreadInitComp);
	SAFE_CLOSE_HANDLE(hTerminateRequest);
}

HANDLE CDecodeProc::CreateThread(LPTHREAD_START_ROUTINE proc, LPVOID param, int priority)
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

	DWORD ret = WaitForMultipleObjectsWithMessageLoop(2, h, FALSE, INFINITE);
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

void CDecodeProc::TerminateThread(void)
{
	// スレッド終了
	if (hThread) {
		::SetEvent(hTerminateRequest);
		::WaitForSingleObject(hThread, INFINITE);
		SAFE_CLOSE_HANDLE(hThread);
	}
}

void CDecodeProc::NotifyThreadStarted(void)
{
	::SetEvent(hThreadInitComp);
}

CDecodeProc::enumDecodeWaitStatus CDecodeProc::WaitRequest(DWORD msec, HANDLE hRecvEvent)
{
	HANDLE h[2] = {
		hTerminateRequest,
		hRecvEvent
	};

	return (enumDecodeWaitStatus)WaitForMultipleObjectsWithMessageLoop(2, h, FALSE, msec);
}
