#pragma once

#include <Windows.h>

class CCOMProc {
public:
	enum class enumCOMRequest {
		eCOMReqNone = 0,
		eCOMReqOpenTuner,
		eCOMReqCloseTuner,
		eCOMReqSetChannel,
		eCOMReqGetSignalLevel,
		eCOMReqIsTunerOpening,
		eCOMReqGetCurSpace,
		eCOMReqGetCurChannel,
	};

	enum class enumCOMWaitStatus : DWORD {
		eCOMWaitFailed = WAIT_FAILED,
		eCOMWaitTerminateRequest = WAIT_OBJECT_0,
		eCOMWaitRequestEvent = WAIT_OBJECT_0 + 1,
		eCOMWaitTimeout = WAIT_TIMEOUT,
	};

	struct COMReqParamSetChannel {
		DWORD dwSpace;
		DWORD dwChannel;
	};

	union COMReqParm {
		COMReqParamSetChannel SetChannel;
	};

	union COMReqRetVal {
		BOOL OpenTuner;
		BOOL SetChannel;
		float GetSignalLevel;
		BOOL IsTunerOpening;
		DWORD GetCurSpace;
		DWORD GetCurChannel;
	};

	struct COMReqArgs {
		enumCOMRequest nRequest;	// リクエスト
		COMReqParm uParam;			// パラメータ
		COMReqRetVal uRetVal;		// 戻り値
		COMReqArgs(void)
			: nRequest(enumCOMRequest::eCOMReqNone),
			uParam(),
			uRetVal()
		{
		}
	};

public:
	CCOMProc(void);
	~CCOMProc(void);
	HANDLE CreateThread(LPTHREAD_START_ROUTINE proc, LPVOID param, int priority);
	void TerminateThread(void);
	void NotifyThreadStarted(void);
	BOOL RequestCOMReq(COMReqArgs* args);
	enumCOMWaitStatus WaitRequest(DWORD msec, enumCOMRequest* req, COMReqParm* param);
	void NotifyComplete(COMReqRetVal val);
	BOOL CheckTick(void);
	void ResetWatchDog(void);
	BOOL CheckSignalLockErr(BOOL state, DWORD threshold);
	BOOL CheckBitRateErr(BOOL state, DWORD threshold);
	void SetReLockChannel(void);
	void ResetReLockChannel(void);
	BOOL NeedReLockChannel(void);
	BOOL CheckReLockFailCount(unsigned int threshold);
	void SetReOpenTuner(DWORD space, DWORD channel);
	void ResetReOpenTuner(void);
	BOOL NeedReOpenTuner(void);
	DWORD GetReOpenSpace(void);
	DWORD GetReOpenChannel(void);
	void ClearReOpenChannel(void);
	BOOL CheckReOpenChannel(void);

	// チューニングスペース番号不明
	static constexpr DWORD SPACE_INVALID = 0xFFFFFFFFUL;

	// チャンネル番号不明
	static constexpr DWORD CHANNEL_INVALID = 0xFFFFFFFFUL;

private:
	HANDLE hThread;					// スレッドハンドル
	HANDLE hThreadInitComp;			// スレッド初期化完了通知
	HANDLE hReqEvent;				// COMProcスレッドへのコマンド実行要求
	HANDLE hEndEvent;				// COMProcスレッドからのコマンド完了通知
	CRITICAL_SECTION csLock;		// 排他用
	enumCOMRequest nRequest;		// リクエスト
	COMReqParm uParam;				// パラメータ
	COMReqRetVal uRetVal;			// 戻り値
	DWORD dwTick;					// 現在のTickCount
	DWORD dwTickLastCheck;			// 最後に異常監視の確認を行ったTickCount
	DWORD dwTickSignalLockErr;		// SignalLockの異常発生TickCount
	DWORD dwTickBitRateErr;			// BitRateの異常発生TckCount
	BOOL bSignalLockErr;			// SignalLockの異常発生中Flag
	BOOL bBitRateErr;				// BitRateの異常発生中Flag
	BOOL bDoReLockChannel;			// チャンネルロック再実行中
	BOOL bDoReOpenTuner;			// チューナー再オープン中
	unsigned int nReLockFailCount;	// Re-LockChannel失敗回数
	DWORD dwReOpenSpace;			// チューナー再オープン時のカレントチューニングスペース番号退避
	DWORD dwReOpenChannel;			// チューナー再オープン時のカレントチャンネル番号退避
	HANDLE hTerminateRequest;		// スレッド終了要求
};
