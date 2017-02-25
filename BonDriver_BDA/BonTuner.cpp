// BonTuner.cpp: CBonTuner クラスのインプリメンテーション
//
//////////////////////////////////////////////////////////////////////

#include <Windows.h>
#include <stdio.h>

#include "BonTuner.h"

#include "common.h"

#include "tswriter.h"

#include <iostream>
#include <DShow.h>

// KSCATEGORY_...
#include <ks.h>
#pragma warning (push)
#pragma warning (disable: 4091)
#include <ksmedia.h>
#pragma warning (pop)
#include <bdatypes.h>
#include <bdamedia.h>

// bstr_t
#include <comdef.h>

#include "WaitWithMsg.h"

#pragma comment(lib, "Strmiids.lib")
#pragma comment(lib, "ksproxy.lib")

#ifdef _DEBUG
#pragma comment(lib, "strmbasd.lib")
#else
#pragma comment(lib, "strmbase.lib")
#endif

#pragma comment(lib, "winmm.lib")

FILE *g_fpLog = NULL;

//////////////////////////////////////////////////////////////////////
// 定数等定義
//////////////////////////////////////////////////////////////////////

// TS Writerの名称:AddFilter時に名前を登録するだけなので何でもよい
static const WCHAR *FILTER_GRAPH_NAME_TSWRITER	= L"TS Writer";

// MPEG2 Demultiplexerの名称:AddFilter時に名前を登録するだけなので何でもよい
static const WCHAR *FILTER_GRAPH_NAME_DEMUX = L"MPEG2 Demultiplexer";

// MPEG2 TIFの名称:CLSIDだけでは特定できないのでこの名前と一致するものを使用する
static const WCHAR *FILTER_GRAPH_NAME_TIF = L"BDA MPEG2 Transport Information Filter";

//////////////////////////////////////////////////////////////////////
// 静的メンバ変数
//////////////////////////////////////////////////////////////////////

// Dllのモジュールハンドル
HMODULE CBonTuner::st_hModule = NULL;

// 作成されたCBontunerインスタンスの一覧
list<CBonTuner*> CBonTuner::st_InstanceList;

// st_InstanceList操作用
CRITICAL_SECTION CBonTuner::st_LockInstanceList;

// CBonTunerで使用する偏波種類番号とPolarisation型のMapping
const Polarisation CBonTuner::PolarisationMapping[] = {
	BDA_POLARISATION_NOT_DEFINED,
	BDA_POLARISATION_LINEAR_H,
	BDA_POLARISATION_LINEAR_V,
	BDA_POLARISATION_CIRCULAR_L,
	BDA_POLARISATION_CIRCULAR_R
};

const WCHAR CBonTuner::PolarisationChar[] = {
	L'\0',
	L'H',
	L'V',
	L'L',
	L'R'
};

const CBonTuner::TUNER_SPECIAL_DLL CBonTuner::aTunerSpecialData [] = {
	// ここはプログラマしかいじらないと思うので、プログラム中でGUID を小文字に正規化しないので、
	// 追加する場合は、GUIDは小文字で書いてください

	/* TBS6980A */
	{ L"{e9ead02c-8b8c-4d9b-97a2-2ec0324360b1}", L"TBS" },

	/* TBS6980B, Prof 8000 */
	{ L"{ed63ec0b-a040-4c59-bc9a-59b328a3f852}", L"TBS" },

	/* Prof 7300, 7301, TBS 8920 */ 
	{ L"{91b0cc87-9905-4d65-a0d1-5861c6f22cbf}", L"TBS" },	// 7301 は固有関数でなくてもOKだった

	/* TBS 6920 */
	{ L"{ed63ec0b-a040-4c59-bc9a-59b328a3f852}", L"TBS" },

	/* Prof Prof 7500, Q-BOX II */ 
	{ L"{b45b50ff-2d09-4bf2-a87c-ee4a7ef00857}", L"TBS" },

	/* DVBWorld 2002, 2004, 2006 */
	{ L"{4c807f36-2db7-44ce-9582-e1344782cb85}", L"DVBWorld" },

	/* DVBWorld 210X, 2102X, 2104X */
	{ L"{5a714cad-60f9-4124-b922-8a0557b8840e}", L"DVBWorld" },

	/* DVBWorld 2005 */
	{ L"{ede18552-45e6-469f-93b5-27e94296de38}", L"DVBWorld" }, // 2005 は固有関数は必要ないかも

	{ L"", L"" }, // terminator
};

//////////////////////////////////////////////////////////////////////
// インスタンス生成メソッド
//////////////////////////////////////////////////////////////////////
#pragma warning(disable : 4273)
extern "C" __declspec(dllexport) IBonDriver * CreateBonDriver()
{
	return (IBonDriver *) new CBonTuner;
}
#pragma warning(default : 4273)

//////////////////////////////////////////////////////////////////////
// 構築/消滅
//////////////////////////////////////////////////////////////////////
CBonTuner::CBonTuner()
	: m_nToneWait(100),
	m_nLockWait(2000),
	m_nLockWaitDelay(0),
	m_nLockWaitRetry(0),
	m_bLockTwice(FALSE),
	m_nLockTwiceDelay(100),
	m_nWatchDogSignalLocked(0),
	m_nWatchDogBitRate(0),
	m_nReOpenWhenGiveUpReLock(0),
	m_bTryAnotherTuner(FALSE),
	m_bBackgroundChannelLock(FALSE),
	m_nSignalLevelCalcType(0),
	m_bSignalLevelGetTypeSS(FALSE),
	m_bSignalLevelGetTypeTuner(FALSE),
	m_bSignalLevelGetTypeBR(FALSE),
	m_bSignalLevelNeedStrength(FALSE),
	m_bSignalLevelNeedQuality(FALSE),
	m_bSignalLevelCalcTypeMul(FALSE),
	m_bSignalLevelCalcTypeAdd(FALSE),
	m_fStrengthCoefficient(1),
	m_fQualityCoefficient(1),
	m_fStrengthBias(0),
	m_fQualityBias(0),
	m_nSignalLockedJudgeType(1),
	m_bSignalLockedJudgeTypeSS(FALSE),
	m_bSignalLockedJudgeTypeTuner(FALSE),
	m_dwBuffSize(188 * 1024),
	m_dwMaxBuffCount(512),
	m_nWaitTsCount(1),
	m_nWaitTsSleep(100),
	m_bAlwaysAnswerLocked(FALSE),
	m_bReserveUnusedCh(FALSE),
	m_szIniFilePath(L""),
	m_hOnStreamEvent(NULL),
	m_hOnDecodeEvent(NULL),
	m_LastBuff(NULL),
	m_bRecvStarted(FALSE),
	m_hSemaphore(NULL),
	m_pITuningSpace(NULL),
	m_pITuner(NULL),
	m_pNetworkProvider(NULL),
	m_pTunerDevice(NULL),
	m_pCaptureDevice(NULL),
	m_pTsWriter(NULL),
	m_pDemux(NULL),
	m_pTif(NULL),
	m_pIGraphBuilder(NULL),
	m_pIMediaControl(NULL), 
	m_pCTsWriter(NULL),
	m_pIBDA_SignalStatistics(NULL),
	m_pDSFilterEnumTuner(NULL),
	m_pDSFilterEnumCapture(NULL),
	m_nDVBSystemType(eTunerTypeDVBS),
	m_nNetworkProvider(eNetworkProviderAuto),
	m_nDefaultNetwork(1),
	m_bOpened(FALSE),
	m_dwTargetSpace(CBonTuner::SPACE_INVALID),
	m_dwCurSpace(CBonTuner::SPACE_INVALID),
	m_dwTargetChannel(CBonTuner::CHANNEL_INVALID),
	m_dwCurChannel(CBonTuner::CHANNEL_INVALID),
	m_nCurTone(CBonTuner::TONE_UNKNOWN),
	m_hModuleTunerSpecials(NULL),
	m_pIBdaSpecials(NULL),
	m_pIBdaSpecials2(NULL)
{
	// インスタンスリストに自身を登録
	::EnterCriticalSection(&st_LockInstanceList);
	st_InstanceList.push_back(this);
	::LeaveCriticalSection(&st_LockInstanceList);

	setlocale(LC_CTYPE, "ja_JP.SJIS");

	::InitializeCriticalSection(&m_csTSBuff);
	::InitializeCriticalSection(&m_csDecodedTSBuff);

	ReadIniFile();

	m_TsBuff.SetSize(m_dwBuffSize, m_dwMaxBuffCount);
	m_DecodedTsBuff.SetSize(0, m_dwMaxBuffCount);

	// COM処理専用スレッド起動
	m_aCOMProc.hThread = ::CreateThread(NULL, 0, CBonTuner::COMProcThread, this, 0, NULL);
}

CBonTuner::~CBonTuner()
{
	OutputDebug(L"~CBonTuner called.\n");
	CloseTuner();

	// COM処理専用スレッド終了
	if (m_aCOMProc.hThread) {
		::SetEvent(m_aCOMProc.hTerminateRequest);
		::WaitForSingleObject(m_aCOMProc.hThread, INFINITE);
		::CloseHandle(m_aCOMProc.hThread);
		m_aCOMProc.hThread = NULL;
	}

	::DeleteCriticalSection(&m_csDecodedTSBuff);
	::DeleteCriticalSection(&m_csTSBuff);

	// インスタンスリストから自身を削除
	::EnterCriticalSection(&st_LockInstanceList);
	st_InstanceList.remove(this);
	::LeaveCriticalSection(&st_LockInstanceList);
}

/////////////////////////////////////
//
// IBonDriver2 APIs
//
/////////////////////////////////////

const BOOL CBonTuner::OpenTuner(void)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	BOOL ret = FALSE;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqOpenTuner;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.OpenTuner;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const BOOL CBonTuner::_OpenTuner(void)
{
	if (m_bOpened)
		return TRUE;

	HRESULT hr;

	do {
		// フィルタグラフの作成
		if (FAILED(hr = InitializeGraphBuilder()))
			break;

		// チューニングスペースの読込
		if (FAILED(hr = CreateTuningSpace()))
			break;

		// ネットワークプロバイダ
		if (FAILED(hr = LoadNetworkProvider()))
			break;

		// チューニングスペース初期化
		if (FAILED(hr = InitTuningSpace()))
			break;

		// ロードすべきチューナ・キャプチャのリスト作成
		if (m_UsableTunerCaptureList.empty() && FAILED(hr = InitDSFilterEnum()))
			break;

		// チューナ・キャプチャ以後の構築と実行
		if (FAILED(hr = LoadAndConnectDevice()))
			break;

		OutputDebug(L"Build graph Successfully.\n");

		if (m_bSignalLockedJudgeTypeSS || m_bSignalLevelGetTypeSS) {
			// チューナの信号状態取得用インターフェースの取得（失敗しても続行）
			hr = LoadTunerSignalStatistics();
		}

		// TS受信イベント作成
		m_hOnStreamEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

		// Decodeイベント作成
		m_hOnDecodeEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

		// Decode処理専用スレッド起動
		m_aDecodeProc.hThread = ::CreateThread(NULL, 0, CBonTuner::DecodeProcThread, this, 0, NULL);

		// コールバック関数セット
		StartRecv();

		m_bOpened = TRUE;

		return TRUE;

	} while(0);

	// ここに到達したということは何らかのエラーで失敗した
	_CloseTuner();

	return FALSE;
}

void CBonTuner::CloseTuner(void)
{
	if (m_aCOMProc.hThread == NULL)
		return;

	DWORD dw;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqCloseTuner;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return;
}

#pragma warning (push)
#pragma warning (disable: 4702)
void CBonTuner::_CloseTuner(void)
{
	m_bOpened = FALSE;

	// グラフ停止
	StopGraph();

	// コールバック関数停止
	StopRecv();

	// Decode処理専用スレッド終了
	if (m_aDecodeProc.hThread) {
		::SetEvent(m_aDecodeProc.hTerminateRequest);
		WaitForSingleObjectWithMessageLoop(m_aDecodeProc.hThread, INFINITE);
		::CloseHandle(m_aDecodeProc.hThread);
		m_aDecodeProc.hThread = NULL;
	}

	// Decodeイベント開放
	if (m_hOnDecodeEvent) {
		::CloseHandle(m_hOnDecodeEvent);
		m_hOnDecodeEvent = NULL;
	}

	// TS受信イベント解放
	if (m_hOnStreamEvent) {
		::CloseHandle(m_hOnStreamEvent);
		m_hOnStreamEvent = NULL;
	}

	// バッファ解放
	PurgeTsStream();
	SAFE_DELETE(m_LastBuff);

	// チューナの信号状態取得用インターフェース解放
	UnloadTunerSignalStatistics();

	// グラフ解放
	CleanupGraph();

	m_dwTargetSpace = m_dwCurSpace = CBonTuner::SPACE_INVALID;
	m_dwTargetChannel = m_dwCurChannel = CBonTuner::CHANNEL_INVALID;
	m_nCurTone = CBonTuner::TONE_UNKNOWN;

	if (m_hSemaphore) {
		try {
			::ReleaseSemaphore(m_hSemaphore, 1, NULL);
			::CloseHandle(m_hSemaphore);
			m_hSemaphore = NULL;
		} catch (...) {
			OutputDebug(L"Exception in ReleaseSemaphore.\n");
		}
	}

	return;
}
#pragma warning (pop)

const BOOL CBonTuner::SetChannel(const BYTE byCh)
{
	// IBonDriver (not IBonDriver2) 用インターフェース; obsolete?
	return SetChannel(0UL, DWORD(byCh));
}

const float CBonTuner::GetSignalLevel(void)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	float ret = 0.0F;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqGetSignalLevel;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.GetSignalLevel;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const float CBonTuner::_GetSignalLevel(void)
{
	if (!m_bOpened)
		return -1.0F;

	HRESULT hr;
	float f = 0.0F;

	// ビットレートを返す場合
	if (m_bSignalLevelGetTypeBR) {
		return m_BitRate.GetRate();
	}

	// IBdaSpecials2固有関数があれば丸投げ
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->GetSignalStrength(&f)) != E_NOINTERFACE) {
		return f;
	}

	//   get_SignalQuality 信号の品質を示す 1 ～ 100 の値を取得する。
	//   get_SignalStrength デシベル単位の信号の強度を示す値を取得する。 
	int nStrength;
	int nQuality;
	int nLock;

	if (m_dwTargetChannel == CBonTuner::CHANNEL_INVALID)
		// SetChannel()が一度も呼ばれていない場合は0を返す
		return 0;

	GetSignalState(&nStrength, &nQuality, &nLock);
	if (!nLock)
		// Lock出来ていない場合は0を返す
		return 0;
	if (nStrength < 0 && m_bSignalLevelNeedStrength)
		// Strengthは-1を返す場合がある
		return (float)nStrength;
	float s = 0.0F;
	float q = 0.0F;
	if (m_bSignalLevelNeedStrength)
		s = float(nStrength) / m_fStrengthCoefficient + m_fStrengthBias;
	if (m_bSignalLevelNeedQuality)
		q = float(nQuality) / m_fQualityCoefficient + m_fQualityBias;

	if (m_bSignalLevelCalcTypeMul)
		return s * q;
	return s + q;
}

const DWORD CBonTuner::WaitTsStream(const DWORD dwTimeOut)
{
	if( m_hOnDecodeEvent == NULL ){
		return WAIT_ABANDONED;
	}

	DWORD dwRet;
	if (m_nWaitTsSleep) {
		// WaitTsSleep が指定されている場合
		dwRet = ::WaitForSingleObject(m_hOnDecodeEvent, 0);
		// イベントがシグナル状態でなければ指定時間待機する
		if (dwRet != WAIT_TIMEOUT)
			return dwRet;

		::Sleep(m_nWaitTsSleep);
	}

	// イベントがシグナル状態になるのを待つ
	dwRet = ::WaitForSingleObject(m_hOnDecodeEvent, (dwTimeOut)? dwTimeOut : INFINITE);
	return dwRet;
}

const DWORD CBonTuner::GetReadyCount(void)
{
	return (DWORD)m_DecodedTsBuff.Size();
}

const BOOL CBonTuner::GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	BYTE *pSrc = NULL;
	if (GetTsStream(&pSrc, pdwSize, pdwRemain)) {
		if (*pdwSize)
			::CopyMemory(pDst, pSrc, *pdwSize);
		return TRUE;
	}
	return FALSE;
}

const BOOL CBonTuner::GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	SAFE_DELETE(m_LastBuff);
	BOOL bRet = TRUE;
	m_LastBuff = m_DecodedTsBuff.Get();
	if (m_LastBuff) {
		*pdwSize = m_LastBuff->dwSize;
		*ppDst = m_LastBuff->pbyBuff;
		*pdwRemain = (DWORD)m_DecodedTsBuff.Size();
	}
	else {
		*pdwSize = 0;
		*ppDst = NULL;
		*pdwRemain = 0;
		bRet = FALSE;
	}
	return bRet;
}

void CBonTuner::PurgeTsStream(void)
{
	// m_LastBuff は参照されている可能性があるので delete しない

	// 受信TSバッファ
	m_TsBuff.Purge();

	// デコード後TSバッファ
	m_DecodedTsBuff.Purge();

	// ビットレート計算用クラス
	m_BitRate.Clear();
}

LPCTSTR CBonTuner::GetTunerName(void)
{
	return m_aTunerParam.sTunerName.c_str();
}

const BOOL CBonTuner::IsTunerOpening(void)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	BOOL ret = FALSE;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqIsTunerOpening;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.IsTunerOpening;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const BOOL CBonTuner::_IsTunerOpening(void)
{
	return m_bOpened;
}

LPCTSTR CBonTuner::EnumTuningSpace(const DWORD dwSpace)
{
	if (dwSpace < m_TuningData.dwNumSpace) {
		map<unsigned int, TuningSpaceData*>::iterator it = m_TuningData.Spaces.find(dwSpace);
		if (it != m_TuningData.Spaces.end())
			return it->second->sTuningSpaceName.c_str();
		else
#ifdef UNICODE
			return _T("-");
#else
			return "-";
#endif
	}
	return NULL;
}

LPCTSTR CBonTuner::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel)
{
	map<unsigned int, TuningSpaceData*>::iterator it = m_TuningData.Spaces.find(dwSpace);
	if (it != m_TuningData.Spaces.end()) {
		if (dwChannel < it->second->dwNumChannel) {
			map<unsigned int, ChData*>::iterator it2 = it->second->Channels.find(dwChannel);
			if (it2 != it->second->Channels.end())
				return it2->second->sServiceName.c_str();
			else
#ifdef UNICODE
				return _T("----");
#else
				return "----";
#endif
		}
	}
	return NULL;
}

const BOOL CBonTuner::SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	BOOL ret = FALSE;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqSetChannel;
	m_aCOMProc.uParam.SetChannel.dwSpace = dwSpace;
	m_aCOMProc.uParam.SetChannel.dwChannel = dwChannel;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.SetChannel;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const BOOL CBonTuner::_SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	HRESULT hr;

	OutputDebug(L"SetChannel(%d, %d)\n", dwSpace, dwChannel);

	m_dwTargetSpace = m_dwCurSpace = CBonTuner::SPACE_INVALID;
	m_dwTargetChannel = m_dwCurChannel = CBonTuner::CHANNEL_INVALID;

	map<unsigned int, TuningSpaceData*>::iterator it = m_TuningData.Spaces.find(dwSpace);
	if (it == m_TuningData.Spaces.end()) {
		OutputDebug(L"    Invalid channel space.\n");
		return FALSE;
	}

	if (dwChannel >= it->second->dwNumChannel) {
		OutputDebug(L"    Invalid channel number.\n");
		return FALSE;
	}

	map<unsigned int, ChData*>::iterator it2 = it->second->Channels.find(dwChannel);
	if (it2 == it->second->Channels.end()) {
		OutputDebug(L"    Reserved channel number.\n");
		return FALSE;
	}

	if (!m_bOpened) {
		OutputDebug(L"    Tuner not opened.\n");
		return FALSE;
	}

	m_bRecvStarted = FALSE;
	PurgeTsStream();
	ChData * Ch = it2->second;
	m_LastTuningParam.Frequency = Ch->Frequency;
	m_LastTuningParam.Polarisation = PolarisationMapping[Ch->Polarisation];
	m_LastTuningParam.Antenna = &m_aSatellite[Ch->Satellite].Polarisation[Ch->Polarisation];
	m_LastTuningParam.Modulation = &m_aModulationType[Ch->ModulationType];
	m_LastTuningParam.ONID = Ch->ONID;
	m_LastTuningParam.TSID = Ch->TSID;
	m_LastTuningParam.SID = Ch->SID;

	BOOL bRet = LockChannel(&m_LastTuningParam, m_bLockTwice && Ch->LockTwiceTarget);

	// IBdaSpecialsで追加の処理が必要なら行う
	if (m_pIBdaSpecials2)
		hr = m_pIBdaSpecials2->PostLockChannel(&m_LastTuningParam);

	SleepWithMessageLoop(100);
	PurgeTsStream();
	m_bRecvStarted = TRUE;

	// SetChannel()を試みたチューニングスペース番号とチャンネル番号
	m_dwTargetSpace = dwSpace;
	m_dwTargetChannel = dwChannel;

	if (bRet) {
		OutputDebug(L"SetChannel success.\n");
		m_dwCurSpace = dwSpace;
		m_dwCurChannel = dwChannel;
		return TRUE;
	}
	// m_byCurTone = CBonTuner::TONE_UNKNOWN;

	OutputDebug(L"SetChannel failed.\n");
	if (m_bAlwaysAnswerLocked)
		return TRUE;
	return FALSE;
}

const DWORD CBonTuner::GetCurSpace(void)
{
	if (m_aCOMProc.hThread == NULL)
		return CBonTuner::SPACE_INVALID;

	DWORD dw;
	DWORD ret = CBonTuner::SPACE_INVALID;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqGetCurSpace;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.GetCurSpace;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const DWORD CBonTuner::_GetCurSpace(void)
{
	if (m_bAlwaysAnswerLocked)
		return m_dwTargetSpace;

	return m_dwCurSpace;
}

const DWORD CBonTuner::GetCurChannel(void)
{
	if (m_aCOMProc.hThread == NULL)
		return CBonTuner::CHANNEL_INVALID;

	DWORD dw;
	DWORD ret = CBonTuner::CHANNEL_INVALID;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqGetCurChannel;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.GetCurChannel;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const DWORD CBonTuner::_GetCurChannel(void)
{
	if (m_bAlwaysAnswerLocked)
		return m_dwTargetChannel;

	return m_dwCurChannel;
}

void CBonTuner::Release(void)
{
	OutputDebug(L"CBonTuner::Release called.\n");

	delete this;
}

DWORD WINAPI CBonTuner::COMProcThread(LPVOID lpParameter)
{
	BOOL terminate = FALSE;
	CBonTuner* pSys = (CBonTuner*) lpParameter;
	COMProc* pCOMProc = &pSys->m_aCOMProc;
	HRESULT hr;

	OutputDebug(L"COMProcThread: Thread created.\n");

	// COM初期化
	hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);

	HANDLE h[2] = {
		pCOMProc->hTerminateRequest,
		pCOMProc->hReqEvent
	};

	while (!terminate) {
		DWORD ret = WaitForMultipleObjectsWithMessageLoop(2, h, FALSE, 1000);
		switch (ret)
		{
		case WAIT_OBJECT_0:
			terminate = TRUE;
			break;

		case WAIT_OBJECT_0 + 1:
			switch (pCOMProc->nRequest)
			{

			case eCOMReqOpenTuner:
				pCOMProc->uRetVal.OpenTuner = pSys->_OpenTuner();
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqCloseTuner:
				// OpenTuner・LockChannelの再試行中なら中止
				pCOMProc->ResetReOpenTuner();
				pCOMProc->ResetReLockChannel();

				pSys->_CloseTuner();
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqSetChannel:
				// 異常検知監視タイマー初期化
				pCOMProc->ResetWatchDog();

				// LockChannelの再試行中なら中止
				pCOMProc->ResetReLockChannel();

				// OpenTunerの再試行中ならFALSEを返す
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->ClearReOpenChannel();
					pCOMProc->uRetVal.SetChannel = FALSE;
				}
				else {
					pCOMProc->uRetVal.SetChannel = pSys->_SetChannel(pCOMProc->uParam.SetChannel.dwSpace, pCOMProc->uParam.SetChannel.dwChannel);
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqGetSignalLevel:
				// OpenTunerの再試行中なら0を返す
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.GetSignalLevel = 0.0F;
				}
				else {
					pCOMProc->uRetVal.GetSignalLevel = pSys->_GetSignalLevel();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqIsTunerOpening:
				// OpenTunerの再試行中ならTRUEを返す
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.IsTunerOpening = TRUE;
				}
				else {
					pCOMProc->uRetVal.IsTunerOpening = pSys->_IsTunerOpening();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqGetCurSpace:
				// OpenTunerの再試行中なら退避値を返す
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.GetCurSpace = pCOMProc->dwReOpenSpace;
				}
				else {
					pCOMProc->uRetVal.GetCurSpace = pSys->_GetCurSpace();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqGetCurChannel:
				// OpenTunerの再試行中なら退避値を返す
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.GetCurChannel = pCOMProc->dwReOpenChannel;
				}
				else {
					pCOMProc->uRetVal.GetCurChannel = pSys->_GetCurChannel();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			default:
				break;
			}
			break;

		case WAIT_TIMEOUT:
			break;

		case WAIT_FAILED:
		default:
			DWORD err = ::GetLastError();
			OutputDebug(L"COMProcThread: Unknown error (ret=%d, LastError=0x%08x).\n", ret, err);
			terminate = TRUE;
			break;
		}

		if (terminate)
			break;

		// 異常検知＆リカバリー
		// 1000ms毎処理
		if (pCOMProc->CheckTick()) {

			// SetChannel()失敗時のバックグランドCH切替開始
			if (pSys->m_bBackgroundChannelLock && pSys->m_dwCurChannel == CBonTuner::CHANNEL_INVALID && pSys->m_dwTargetChannel != CBonTuner::CHANNEL_INVALID) {
				OutputDebug(L"COMProcThread: Background retry.\n");
				pCOMProc->SetReLockChannel();
			}

			// 異常検知
			if (!pCOMProc->bDoReLockChannel && !pCOMProc->bDoReOpenTuner && pSys->m_dwCurChannel != CBonTuner::CHANNEL_INVALID) {

				// SignalLockの状態確認
				if (pSys->m_nWatchDogSignalLocked != 0) {
					int lock = 0;
					pSys->GetSignalState(NULL, NULL, &lock);
					if (pCOMProc->CheckSignalLockErr(lock, pSys->m_nWatchDogSignalLocked * 1000)) {
						// チャンネルロック再実行
						OutputDebug(L"COMProcThread: WatchDogSignalLocked time is up.\n");
						pCOMProc->SetReLockChannel();
					}
				} // SignalLockの状態確認

				// BitRate確認
				if (pSys->m_nWatchDogBitRate != 0) {
					if (pCOMProc->CheckBitRateErr((pSys->m_BitRate.GetRate() > 0.0F), pSys->m_nWatchDogBitRate * 1000)) {
						// チャンネルロック再実行
						OutputDebug(L"COMProcThread: WatchDogBitRate time is up.\n");
						pCOMProc->SetReLockChannel();
					}
				} // BitRate確認
			} // 異常検知

			// CH切替動作試行後のOpenTuner再実行
			if (pCOMProc->bDoReOpenTuner) {
				// OpenTuner再実行
				pSys->_CloseTuner();
				if (pSys->_OpenTuner() && (!pCOMProc->CheckReOpenChannel() || pSys->_SetChannel(pCOMProc->dwReOpenSpace, pCOMProc->dwReOpenChannel))) {
					// OpenTunerに成功し、SetChannnelに成功もしくは必要ない
					OutputDebug(L"COMProcThread: Re-OpenTuner SUCCESS.\n");
					pCOMProc->ResetReOpenTuner();
				}
				else {
					// 失敗...そのまま次回もチャレンジする
					OutputDebug(L"COMProcThread: Re-OpenTuner FAILED.\n");
				}
			} // CH切替動作試行後のOpenTuner再実行

			// 異常検知後チャンネルロック再実行
			if (!pCOMProc->bDoReOpenTuner && pCOMProc->bDoReLockChannel) {
				// チャンネルロック再実行
				if (pSys->LockChannel(&pSys->m_LastTuningParam, FALSE)) {
					// LockChannelに成功した
					OutputDebug(L"COMProcThread: Re-LockChannel SUCCESS.\n");
					pCOMProc->ResetReLockChannel();
				}
				else {
					// LockChannel失敗
					OutputDebug(L"COMProcThread: Re-LockChannel FAILED.\n");
					if (pSys->m_nReOpenWhenGiveUpReLock != 0) {
						// CH切替動作試行回数設定値が0以外
						if (pCOMProc->CheckReLockFailCount(pSys->m_nReOpenWhenGiveUpReLock)) {
							// CH切替動作試行回数を超えたのでOpenTuner再実行
							OutputDebug(L"COMProcThread: ReOpenWhenGiveUpReLock count is up.\n");
							pCOMProc->SetReOpenTuner(pSys->m_dwTargetSpace, pSys->m_dwTargetChannel);
							pCOMProc->ResetReLockChannel();
						}
					}
				}
			} // 異常検知後チャンネルロック再実行
		} // 1000ms毎処理
	} // while (!terminate)

	// DSフィルター列挙とチューナ・キャプチャのリストを削除
	SAFE_DELETE(pSys->m_pDSFilterEnumTuner);
	SAFE_DELETE(pSys->m_pDSFilterEnumCapture);
	pSys->m_UsableTunerCaptureList.clear();

	::CoUninitialize();
	OutputDebug(L"COMProcThread: Thread terminated.\n");

	return 0;
}

DWORD WINAPI CBonTuner::DecodeProcThread(LPVOID lpParameter)
{
	BOOL terminate = FALSE;
	CBonTuner* pSys = (CBonTuner*)lpParameter;

	BOOL bNeedDecode = FALSE;

	HRESULT hr;

	OutputDebug(L"DecodeProcThread: Thread created.\n");

	// COM初期化
	hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);

	// IBdaSpecialsによるデコード処理が必要かどうか
	BOOL b = FALSE;
	if (pSys->m_pIBdaSpecials2 && SUCCEEDED(hr = pSys->m_pIBdaSpecials2->IsDecodingNeeded(&b))) {
		if (b)
			bNeedDecode = TRUE;
	}
	OutputDebug(L"DecodeProcThread: Detected IBdaSpecials decoding=%d.\n", bNeedDecode);

	HANDLE h[2] = {
		pSys->m_aDecodeProc.hTerminateRequest,
		pSys->m_hOnStreamEvent
	};

	while (!terminate) {
		DWORD remain = pSys->m_BitRate.CheckRate();
		DWORD ret = WaitForMultipleObjectsWithMessageLoop(2, h, FALSE, remain);
		switch (ret)
		{
		case WAIT_OBJECT_0:
			terminate = TRUE;
			break;
		case WAIT_OBJECT_0 + 1:
			{
				// TSバッファからのデータ取得
				while (TS_DATA *pBuff = pSys->m_TsBuff.Get()) {
					// 必要ならばIBdaSpecialsによるデコード処理を行う
					if (bNeedDecode) {
						pSys->m_pIBdaSpecials2->Decode(pBuff->pbyBuff, pBuff->dwSize);
					}

					// 取得したバッファをデコード済みバッファに追加
					pSys->m_DecodedTsBuff.Add(pBuff);

					// 受信イベントセット
					if (pSys->m_DecodedTsBuff.Size() >= pSys->m_nWaitTsCount)
						::SetEvent(pSys->m_hOnDecodeEvent);
				}
			}
			break;
		case WAIT_TIMEOUT:
			break;
		case WAIT_FAILED:
		default:
			DWORD err = ::GetLastError();
			OutputDebug(L"DecodeProcThread: Unknown error (ret=%d, LastError=0x%08x).\n", ret, err);
			terminate = TRUE;
			break;
		}
	}

	::CoUninitialize();
	OutputDebug(L"DecodeProcThread: Thread terminated.\n");

	return 0;
}

int CALLBACK CBonTuner::RecvProc(void* pParam, BYTE* pbData, DWORD dwSize)
{
	CBonTuner* pSys = (CBonTuner*)pParam;

	pSys->m_BitRate.AddRate(dwSize);

	if (pSys->m_bRecvStarted) {
		if (pSys->m_TsBuff.AddData(pbData, dwSize)) {
			::SetEvent(pSys->m_hOnStreamEvent);
		}
	}

	return 0;
}

void CBonTuner::StartRecv(void)
{
	if (m_pCTsWriter)
		m_pCTsWriter->SetCallBackRecv(RecvProc, this);
	m_bRecvStarted = TRUE;
}

void CBonTuner::StopRecv(void)
{
	if (m_pCTsWriter)
		m_pCTsWriter->SetCallBackRecv(NULL, this);
	m_bRecvStarted = FALSE;
}

void CBonTuner::ReadIniFile(void)
{
	// INIファイルのファイル名取得
	::GetModuleFileNameW(st_hModule, m_szIniFilePath, sizeof(m_szIniFilePath) / sizeof(m_szIniFilePath[0]));

	::wcscpy_s(m_szIniFilePath + ::wcslen(m_szIniFilePath) - 3, 4, L"ini");

	WCHAR buf[256];
	WCHAR buf2[256];
	WCHAR buf3[256];
	WCHAR buf4[256];
#ifndef UNICODE
	char charBuf[512];
#endif
	int val;
	wstring strBuf;

	// DebugLogを記録するかどうか
	if (::GetPrivateProfileIntW(L"BONDRIVER", L"DebugLog", 0, m_szIniFilePath)) {
		WCHAR szDebugLogPath[_MAX_PATH + 1];
		::wcscpy_s(szDebugLogPath, _MAX_PATH + 1, m_szIniFilePath);
		::wcscpy_s(szDebugLogPath + ::wcslen(szDebugLogPath) - 3, 4, L"log");
		SetDebugLog(szDebugLogPath);
	}

	//
	// Tuner セクション
	//

	// GUID0 - GUID99: TunerデバイスのGUID ... 指定されなければ見つかった順に使う事を意味する。
	// FriendlyName0 - FriendlyName99: TunerデバイスのFriendlyName ... 指定されなければ見つかった順に使う事を意味する。
	// CaptureGUID0 - CaptureGUID99: CaptureデバイスのGUID ... 指定されなければ接続可能なデバイスを検索する。
	// CaptureFriendlyName0 - CaptureFriendlyName99: CaptureデバイスのFriendlyName ... 指定されなければ接続可能なデバイスを検索する。
	for (unsigned int i = 0; i < MAX_GUID; i++) {
		WCHAR keyname[64];
		::swprintf_s(keyname, 64, L"GUID%d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf, 256, m_szIniFilePath);
		::swprintf_s(keyname, 64, L"FriendlyName%d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf2, 256, m_szIniFilePath);
		::swprintf_s(keyname, 64, L"CaptureGUID%d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf3, 256, m_szIniFilePath);
		::swprintf_s(keyname, 64, L"CaptureFriendlyName%d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf4, 256, m_szIniFilePath);
		if (buf[0] == L'\0' && buf2[0] == L'\0' && buf3[0] == L'\0' && buf4[0] == L'\0') {
			// どれも指定されていない
			if (i == 0) {
				// 番号なしの型式で読込む
				::GetPrivateProfileStringW(L"TUNER", L"GUID", L"", buf, 256, m_szIniFilePath);
				::GetPrivateProfileStringW(L"TUNER", L"FriendlyName", L"", buf2, 256, m_szIniFilePath);
				::GetPrivateProfileStringW(L"TUNER", L"CaptureGUID", L"", buf3, 256, m_szIniFilePath);
				::GetPrivateProfileStringW(L"TUNER", L"CaptureFriendlyName", L"", buf4, 256, m_szIniFilePath);
				// どれも指定されていない場合でも登録
			} else
				break;
		}
		TunerSearchData *sdata = new TunerSearchData(buf, buf2, buf3, buf4);
		m_aTunerParam.Tuner.insert(pair<unsigned int, TunerSearchData*>(i, sdata));
	}

	// TunerデバイスのみでCaptureデバイスが存在しない
	m_aTunerParam.bNotExistCaptureDevice = (BOOL)::GetPrivateProfileIntW(L"TUNER", L"NotExistCaptureDevice", 0, m_szIniFilePath);

	// TunerとCaptureのデバイスインスタンスパスが一致しているかの確認を行うかどうか
	m_aTunerParam.bCheckDeviceInstancePath = (BOOL)::GetPrivateProfileIntW(L"TUNER", L"CheckDeviceInstancePath", 1, m_szIniFilePath);

	// Tuner名: GetTunerNameで返すチューナ名 ... 指定されなければデフォルト名が
	//   使われる。この場合、複数チューナを名前で区別する事はできない
	::GetPrivateProfileStringW(L"TUNER", L"Name", L"DVB-S2", buf, 256, m_szIniFilePath);
#ifdef UNICODE
	m_aTunerParam.sTunerName = buf;
#else
	::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
	m_aTunerParam.sTunerName = charBuf;
#endif

	// チューナ固有関数を使用するかどうか。
	//   以下を INI ファイルで指定可能
	//     "" ... 使用しない; "AUTO" ... AUTO(default)
	//     "DLLName" ... チューナ固有関数の入ったDLL名を直接指定
	::GetPrivateProfileStringW(L"TUNER", L"UseSpecial", L"AUTO", buf, 256, m_szIniFilePath);
	m_aTunerParam.sDLLBaseName = buf;

	// Tone信号切替時のWait時間
	m_nToneWait = ::GetPrivateProfileIntW(L"TUNER", L"ToneSignalWait", 100, m_szIniFilePath);

	// CH切替後のLock確認時間
	m_nLockWait = ::GetPrivateProfileIntW(L"TUNER", L"ChannelLockWait", 2000, m_szIniFilePath);

	// CH切替後のLock確認Delay時間
	m_nLockWaitDelay = ::GetPrivateProfileIntW(L"TUNER", L"ChannelLockWaitDelay", 0, m_szIniFilePath);

	// CH切替後のLock確認Retry回数
	m_nLockWaitRetry = ::GetPrivateProfileIntW(L"TUNER", L"ChannelLockWaitRetry", 0, m_szIniFilePath);

	// CH切替動作を強制的に2度行うかどうか
	m_bLockTwice = (BOOL)(::GetPrivateProfileIntW(L"TUNER", L"ChannelLockTwice", 0, m_szIniFilePath));

	// CH切替動作を強制的に2度行う場合のDelay時間
	m_nLockTwiceDelay = ::GetPrivateProfileIntW(L"TUNER", L"ChannelLockTwiceDelay", 100, m_szIniFilePath);

	// SignalLockの異常検知時間(秒)
	m_nWatchDogSignalLocked = ::GetPrivateProfileIntW(L"TUNER", L"WatchDogSignalLocked", 0, m_szIniFilePath);

	// BitRateの異常検知時間(秒)
	m_nWatchDogBitRate = ::GetPrivateProfileIntW(L"TUNER", L"WatchDogBitRate", 0, m_szIniFilePath);

	// 異常検知時、チューナの再オープンを試みるまでのCH切替動作試行回数
	m_nReOpenWhenGiveUpReLock = ::GetPrivateProfileIntW(L"TUNER", L"ReOpenWhenGiveUpReLock", 0, m_szIniFilePath);

	// チューナの再オープンを試みる場合に別のチューナを優先して検索するかどうか
	m_bTryAnotherTuner = (BOOL)(::GetPrivateProfileIntW(L"TUNER", L"TryAnotherTuner", 0, m_szIniFilePath));

	// CH切替に失敗した場合に、異常検知時同様バックグランドでCH切替動作を行うかどうか
	m_bBackgroundChannelLock = (BOOL)(::GetPrivateProfileIntW(L"TUNER", L"BackgroundChannelLock", 0, m_szIniFilePath));

	// Tuning Space名（互換用）
	::GetPrivateProfileStringW(L"TUNER", L"TuningSpaceName", L"スカパー", buf, 64, m_szIniFilePath);
	wstring sTempTuningSpaceName = buf;

	// SignalLevel 算出方法
	//   0 .. IBDA_SignalStatistics::get_SignalStrengthで取得した値 ÷ StrengthCoefficientで指定した数値 ＋ StrengthBiasで指定した数値
	//   1 .. IBDA_SignalStatistics::get_SignalQualityで取得した値 ÷ QualityCoefficientで指定した数値 ＋ QualityBiasで指定した数値
	//   2 .. (IBDA_SignalStatistics::get_SignalStrength ÷ StrengthCoefficient ＋ StrengthBias) × (IBDA_SignalStatistics::get_SignalQuality ÷ QualityCoefficient ＋ QualityBias)
	//   3 .. (IBDA_SignalStatistics::get_SignalStrength ÷ StrengthCoefficient ＋ StrengthBias) ＋ (IBDA_SignalStatistics::get_SignalQuality ÷ QualityCoefficient ＋ QualityBias)
	//  10 .. ITuner::get_SignalStrengthで取得したStrength値 ÷ StrengthCoefficientで指定した数値 ＋ StrengthBiasで指定した数値
	//  11 .. ITuner::get_SignalStrengthで取得したQuality値 ÷ QualityCoefficientで指定した数値 ＋ QualityBiasで指定した数値
	//  12 .. (ITuner::get_SignalStrengthのStrength値 ÷ StrengthCoefficient ＋ StrengthBias) × (ITuner::get_SignalStrengthのQuality値 ÷ QualityCoefficient ＋ QualityBias)
	//  13 .. (ITuner::get_SignalStrengthのStrength値 ÷ StrengthCoefficient ＋ StrengthBias) ＋ (ITuner::get_SignalStrengthのQuality値 ÷ QualityCoefficient ＋ QualityBias)
	// 100 .. ビットレート値(Mibps)
	m_nSignalLevelCalcType = ::GetPrivateProfileIntW(L"TUNER", L"SignalLevelCalcType", 0, m_szIniFilePath);
	if (m_nSignalLevelCalcType >= 0 && m_nSignalLevelCalcType <= 9)
		m_bSignalLevelGetTypeSS = TRUE;
	if (m_nSignalLevelCalcType >= 10 && m_nSignalLevelCalcType <= 19)
		m_bSignalLevelGetTypeTuner = TRUE;
	if (m_nSignalLevelCalcType == 100)
		m_bSignalLevelGetTypeBR = TRUE;
	if (m_nSignalLevelCalcType == 0 || m_nSignalLevelCalcType == 2 || m_nSignalLevelCalcType == 3 ||
			m_nSignalLevelCalcType == 10 || m_nSignalLevelCalcType == 12 || m_nSignalLevelCalcType == 13)
		m_bSignalLevelNeedStrength = TRUE;
	if (m_nSignalLevelCalcType == 1 || m_nSignalLevelCalcType == 2 || m_nSignalLevelCalcType == 3 ||
			m_nSignalLevelCalcType == 11 || m_nSignalLevelCalcType == 12 || m_nSignalLevelCalcType == 13)
		m_bSignalLevelNeedQuality = TRUE;
	if (m_nSignalLevelCalcType == 2 || m_nSignalLevelCalcType == 12)
		m_bSignalLevelCalcTypeMul = TRUE;
	if (m_nSignalLevelCalcType == 3 || m_nSignalLevelCalcType == 13)
		m_bSignalLevelCalcTypeAdd = TRUE;

	// Strength 値補正係数
	::GetPrivateProfileStringW(L"TUNER", L"StrengthCoefficient", L"1.0", buf, 256, m_szIniFilePath);
	m_fStrengthCoefficient = (float)::_wtof(buf);
	if (m_fStrengthCoefficient == 0.0F)
		m_fStrengthCoefficient = 1.0F;

	// Quality 値補正係数
	::GetPrivateProfileStringW(L"TUNER", L"QualityCoefficient", L"1.0", buf, 256, m_szIniFilePath);
	m_fQualityCoefficient = (float)::_wtof(buf);
	if (m_fQualityCoefficient == 0.0F)
		m_fQualityCoefficient = 1.0F;

	// Strength 値補正バイアス
	::GetPrivateProfileStringW(L"TUNER", L"StrengthBias", L"0.0", buf, 256, m_szIniFilePath);
	m_fStrengthBias = (float)::_wtof(buf);

	// Quality 値補正バイアス
	::GetPrivateProfileStringW(L"TUNER", L"QualityBias", L"0.0", buf, 256, m_szIniFilePath);
	m_fQualityBias = (float)::_wtof(buf);

	// チューニング状態の判断方法
	// 0 .. 常にチューニングに成功している状態として判断する
	// 1 .. IBDA_SignalStatistics::get_SignalLockedで取得した値で判断する
	// 2 .. ITuner::get_SignalStrengthで取得した値で判断する
	m_nSignalLockedJudgeType = ::GetPrivateProfileIntW(L"TUNER", L"SignalLockedJudgeType", 1, m_szIniFilePath);
	if (m_nSignalLockedJudgeType == 1)
		m_bSignalLockedJudgeTypeSS = TRUE;
	if (m_nSignalLockedJudgeType == 2)
		m_bSignalLockedJudgeTypeTuner = TRUE;

	// チューナーの使用するTuningSpaceの種類
	//    1 .. DVB-S/DVB-S2
	//    2 .. DVB-T
	//    3 .. DVB-C
	//    4 .. DVB-T2
	//   11 .. ISDB-S
	//   12 .. ISDB-T
	//   21 .. ATSC
	//   22 .. ATSC Cable
	//   23 .. Digital Cable
	m_nDVBSystemType = (enumTunerType)::GetPrivateProfileIntW(L"TUNER", L"DVBSystemType", 1, m_szIniFilePath);

	// チューナーに使用するNetworkProvider
	//    0 .. 自動
	//    1 .. Microsoft Network Provider
	//    2 .. Microsoft DVB-S Network Provider
	//    3 .. Microsoft DVB-T Network Provider
	//    4 .. Microsoft DVB-C Network Provider
	//    5 .. Microsoft ATSC Network Provider
	m_nNetworkProvider = (enumNetworkProvider)::GetPrivateProfileIntW(L"TUNER", L"NetworkProvider", 0, m_szIniFilePath);

	// 衛星受信パラメータ/変調方式パラメータのデフォルト値
	//    1 .. SPHD
	//    2 .. BS/CS110
	//    3 .. UHF/CATV
	m_nDefaultNetwork = ::GetPrivateProfileIntW(L"TUNER", L"DefaultNetwork", 1, m_szIniFilePath);

	//
	// BonDriver セクション
	//

	// ストリームデータバッファ1個分のサイズ
	// 188×設定数(bytes)
	m_dwBuffSize = 188 * ::GetPrivateProfileIntW(L"BONDRIVER", L"BuffSize", 1024, m_szIniFilePath);

	// ストリームデータバッファの最大個数
	m_dwMaxBuffCount = ::GetPrivateProfileIntW(L"BONDRIVER", L"MaxBuffCount", 512, m_szIniFilePath);

	// WaitTsStream時、指定された個数分のストリームデータバッファが貯まるまで待機する
	// チューナのCPU負荷が高いときは数値を大き目にすると効果がある場合もある
	m_nWaitTsCount = ::GetPrivateProfileIntW(L"BONDRIVER", L"WaitTsCount", 1, m_szIniFilePath);
	if (m_nWaitTsCount < 1)
		m_nWaitTsCount = 1;

	// WaitTsStream時ストリームデータバッファが貯まっていない場合に最低限待機する時間(msec)
	// チューナのCPU負荷が高いときは100msec程度を指定すると効果がある場合もある
	m_nWaitTsSleep = ::GetPrivateProfileIntW(L"BONDRIVER", L"WaitTsSleep", 100, m_szIniFilePath);

	// SetChannel()でチャンネルロックに失敗した場合でもFALSEを返さないようにするかどうか
	m_bAlwaysAnswerLocked = (BOOL)(::GetPrivateProfileIntW(L"BONDRIVER", L"AlwaysAnswerLocked", 0, m_szIniFilePath));

	//
	// Satellite セクション
	//

	// 衛星別受信パラメータ

	// 未設定時用（iniファイルからの読込は行わない）
	m_sSatelliteName[0] = L"not set";						// チャンネル名生成用衛星名称
	// 名称以外はコンストラクタのデフォルト値使用

	// デフォルト値設定
	switch (m_nDefaultNetwork) {
	case 1:
		// SPHD
		// 衛星設定1（JCSAT-3A）
		m_sSatelliteName[1] = L"128.0E";						// チャンネル名生成用衛星名称
		m_aSatellite[1].Polarisation[1].HighOscillator = m_aSatellite[1].Polarisation[1].LowOscillator = 11200000;
																// 垂直偏波時LNB周波数
		m_aSatellite[1].Polarisation[1].Tone = 0;				// 垂直偏波時トーン信号
		m_aSatellite[1].Polarisation[2].HighOscillator = m_aSatellite[1].Polarisation[2].LowOscillator = 11200000;
																// 水平偏波時LNB周波数
		m_aSatellite[1].Polarisation[2].Tone = 0;				// 水平偏波時トーン信号

		// 衛星設定2（JCSAT-4B）
		m_sSatelliteName[2] = L"124.0E";						// チャンネル名生成用衛星名称
		m_aSatellite[2].Polarisation[1].HighOscillator = m_aSatellite[2].Polarisation[1].LowOscillator = 11200000;	
																// 垂直偏波時LNB周波数
		m_aSatellite[2].Polarisation[1].Tone = 1;				// 垂直偏波時トーン信号
		m_aSatellite[2].Polarisation[2].HighOscillator = m_aSatellite[2].Polarisation[2].LowOscillator = 11200000;
																// 水平偏波時LNB周波数
		m_aSatellite[2].Polarisation[2].Tone = 1;				// 水平偏波時トーン信号
		break;

	case 2:
		// BS/CS110
		// 衛星設定1
		m_sSatelliteName[1] = L"BS/CS110";						// チャンネル名生成用衛星名称
		m_aSatellite[1].Polarisation[3].HighOscillator = m_aSatellite[1].Polarisation[3].LowOscillator = 10678000;
																// 垂直偏波時LNB周波数
		m_aSatellite[1].Polarisation[3].Tone = 0;				// 垂直偏波時トーン信号
		m_aSatellite[1].Polarisation[4].HighOscillator = m_aSatellite[1].Polarisation[4].LowOscillator = 10678000;
																// 水平偏波時LNB周波数
		m_aSatellite[1].Polarisation[4].Tone = 0;				// 水平偏波時トーン信号
		break;

	case 3:
		// UHF/CATVは衛星設定不要
		break;
	}

	// 衛星設定1～4の設定を読込
	for (unsigned int satellite = 1; satellite < MAX_SATELLITE; satellite++) {
		WCHAR keyname[64];
		::swprintf_s(keyname, 64, L"Satellite%01dName", satellite);
		::GetPrivateProfileStringW(L"SATELLITE", keyname, L"", buf, 64, m_szIniFilePath);
		if (buf[0] != L'\0') {
			m_sSatelliteName[satellite] = buf;
		}

		// 偏波種類1～4のアンテナ設定を読込
		for (unsigned int polarisation = 1; polarisation < POLARISATION_SIZE; polarisation++) {
			// 局発周波数 (KHz)
			// 全偏波共通での設定があれば読み込む
			::swprintf_s(keyname, 64, L"Satellite%01dOscillator", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator = m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator, m_szIniFilePath);
			::swprintf_s(keyname, 64, L"Satellite%01dHighOscillator", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator, m_szIniFilePath);
			::swprintf_s(keyname, 64, L"Satellite%01dLowOscillator", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].LowOscillator, m_szIniFilePath);
			// 個別設定があれば上書きで読み込む
			::swprintf_s(keyname, 64, L"Satellite%01d%cOscillator", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator = m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator, m_szIniFilePath);
			::swprintf_s(keyname, 64, L"Satellite%01d%cHighOscillator", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator, m_szIniFilePath);
			::swprintf_s(keyname, 64, L"Satellite%01d%cLowOscillator", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].LowOscillator, m_szIniFilePath);

			// LNB切替周波数 (KHz)
			// 全偏波共通での設定があれば読み込む
			::swprintf_s(keyname, 64, L"Satellite%01dLNBSwitch", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch, m_szIniFilePath);
			// 個別設定があれば上書きで読み込む
			::swprintf_s(keyname, 64, L"Satellite%01d%cLNBSwitch", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch, m_szIniFilePath);

			// トーン信号 (0 or 1)
			// 全偏波共通での設定があれば読み込む
			::swprintf_s(keyname, 64, L"Satellite%01dToneSignal", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].Tone
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].Tone, m_szIniFilePath);
			// 個別設定があれば上書きで読み込む
			::swprintf_s(keyname, 64, L"Satellite%01d%cToneSignal", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].Tone
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].Tone, m_szIniFilePath);

			// DiSEqC
			// 全偏波共通での設定があれば読み込む
			::swprintf_s(keyname, 64, L"Satellite%01dDiSEqC", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].DiSEqC
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].DiSEqC, m_szIniFilePath);
			// 個別設定があれば上書きで読み込む
			::swprintf_s(keyname, 64, L"Satellite%01d%cDiSEqC", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].DiSEqC
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].DiSEqC, m_szIniFilePath);
		}
	}

	//
	// Modulation セクション
	//

	// 変調方式別パラメータ（0～3の順なので注意）

	// デフォルト値設定
	switch (m_nDefaultNetwork) {
	case 1:
		//SPHD
		// 変調方式設定0（DVB-S）
		m_sModulationName[0] = L"DVB-S";							// チャンネル名生成用変調方式名称
		m_aModulationType[0].Modulation = BDA_MOD_NBC_QPSK;			// 変調タイプ
		m_aModulationType[0].InnerFEC = BDA_FEC_VITERBI;			// 内部前方誤り訂正タイプ
		m_aModulationType[0].InnerFECRate = BDA_BCC_RATE_3_4;		// 内部FECレート
		m_aModulationType[0].OuterFEC = BDA_FEC_RS_204_188;			// 外部前方誤り訂正タイプ
		m_aModulationType[0].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// 外部FECレート
		m_aModulationType[0].SymbolRate = 21096;					// シンボルレート

		// 変調方式設定1（DVB-S2）
		m_sModulationName[1] = L"DVB-S2";							// チャンネル名生成用変調方式名称
		m_aModulationType[1].Modulation = BDA_MOD_NBC_8PSK;			// 変調タイプ
		m_aModulationType[1].InnerFEC = BDA_FEC_VITERBI;			// 内部前方誤り訂正タイプ
		m_aModulationType[1].InnerFECRate = BDA_BCC_RATE_3_5;		// 内部FECレート
		m_aModulationType[1].OuterFEC = BDA_FEC_RS_204_188;			// 外部前方誤り訂正タイプ
		m_aModulationType[1].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// 外部FECレート
		m_aModulationType[1].SymbolRate = 23303;					// シンボルレート
		break;

	case 2:
		// BS/CS110
		// 変調方式設定0
		m_sModulationName[0] = L"ISDB-S";							// チャンネル名生成用変調方式名称
		m_aModulationType[0].Modulation = BDA_MOD_ISDB_S_TMCC;		// 変調タイプ
		m_aModulationType[0].InnerFEC = BDA_FEC_VITERBI;			// 内部前方誤り訂正タイプ
		m_aModulationType[0].InnerFECRate = BDA_BCC_RATE_2_3;		// 内部FECレート
		m_aModulationType[0].OuterFEC = BDA_FEC_RS_204_188;			// 外部前方誤り訂正タイプ
		m_aModulationType[0].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// 外部FECレート
		m_aModulationType[0].SymbolRate = 28860;					// シンボルレート
		break;

	case 3:
		// UHF/CATV
		// 変調方式設定0
		m_sModulationName[0] = L"ISDB-T";							// チャンネル名生成用変調方式名称
		m_aModulationType[0].Modulation = BDA_MOD_ISDB_T_TMCC;		// 変調タイプ
		m_aModulationType[0].InnerFEC = BDA_FEC_VITERBI;			// 内部前方誤り訂正タイプ
		m_aModulationType[0].InnerFECRate = BDA_BCC_RATE_3_4;		// 内部FECレート
		m_aModulationType[0].OuterFEC = BDA_FEC_RS_204_188;			// 外部前方誤り訂正タイプ
		m_aModulationType[0].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// 外部FECレート
		m_aModulationType[0].SymbolRate = -1;						// シンボルレート
		m_aModulationType[0].BandWidth = 6;							// 帯域幅(MHz)
		break;
	}

	// 変調方式設定0～3の値を読込
	for (unsigned int modulation = 0; modulation < MAX_MODULATION; modulation++) {
		WCHAR keyname[64];
		// チャンネル名生成用変調方式名称
		::swprintf_s(keyname, 64, L"ModulationType%01dName", modulation);
		::GetPrivateProfileStringW(L"MODULATION", keyname, L"", buf, 64, m_szIniFilePath);
		if (buf[0] != L'\0') {
			m_sModulationName[modulation] = buf;
		}

		// 変調タイプ
		::swprintf_s(keyname, 64, L"ModulationType%01dModulation", modulation);
		m_aModulationType[modulation].Modulation
			= (ModulationType)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].Modulation, m_szIniFilePath);

		// 内部前方誤り訂正タイプ
		::swprintf_s(keyname, 64, L"ModulationType%01dInnerFEC", modulation);
		m_aModulationType[modulation].InnerFEC
			= (FECMethod)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].InnerFEC, m_szIniFilePath);

		// 内部FECレート
		::swprintf_s(keyname, 64, L"ModulationType%01dInnerFECRate", modulation);
		m_aModulationType[modulation].InnerFECRate
			= (BinaryConvolutionCodeRate)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].InnerFECRate, m_szIniFilePath);

		// 外部前方誤り訂正タイプ
		::swprintf_s(keyname, 64, L"ModulationType%01dOuterFEC", modulation);
		m_aModulationType[modulation].OuterFEC
			= (FECMethod)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].OuterFEC, m_szIniFilePath);

		// 外部FECレート
		::swprintf_s(keyname, 64, L"ModulationType%01dOuterFECRate", modulation);
		m_aModulationType[modulation].OuterFECRate
			= (BinaryConvolutionCodeRate)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].OuterFECRate, m_szIniFilePath);

		// シンボルレート
		::swprintf_s(keyname, 64, L"ModulationType%01dSymbolRate", modulation);
		m_aModulationType[modulation].SymbolRate
			= (long)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].SymbolRate, m_szIniFilePath);

		// 帯域幅(MHz)
		::swprintf_s(keyname, 64, L"ModulationType%01dBandWidth", modulation);
		m_aModulationType[modulation].BandWidth
			= (long)::GetPrivateProfileIntW(L"MODULATION", keyname, m_aModulationType[modulation].BandWidth, m_szIniFilePath);
	}

	//
	// Channel セクション
	//

	// iniファイルからCH設定を読込む際に
	// 使用されていないCH番号があっても前詰せず確保しておくかどうか
	// 0 .. 使用されてない番号があった場合前詰し連続させる
	// 1 .. 使用されていない番号をそのまま空CHとして確保しておく
	m_bReserveUnusedCh = (BOOL)(::GetPrivateProfileIntW(L"CHANNEL", L"ReserveUnusedCh", 0, m_szIniFilePath));

	map<unsigned int, TuningSpaceData*>::iterator itSpace;
	map<unsigned int, ChData*>::iterator itCh;
	// チューニング空間00～99の設定を読込
	for (DWORD space = 0; space < 100; space++)	{
		DWORD result;
		WCHAR sectionname[64];

		::swprintf_s(sectionname, 64, L"TUNINGSPACE%02d", space);
		result = ::GetPrivateProfileSection(sectionname, buf, 256, m_szIniFilePath);
		if (result <= 0) {
			// TuningSpaceXXのセクションが存在しない場合
			if (space != 0)
				continue;
			// TuningSpace00の時はChannelセクションも見る
			::swprintf_s(sectionname, 64, L"CHANNEL");
		}

		// 既にチューニング空間データが存在する場合はその内容を書き換える
		// 無い場合は空のチューニング空間を作成
		itSpace = m_TuningData.Spaces.find(space);
		if (itSpace == m_TuningData.Spaces.end()) {
			TuningSpaceData *tuningSpaceData = new TuningSpaceData();
			itSpace = m_TuningData.Spaces.insert(m_TuningData.Spaces.begin(), pair<unsigned int, TuningSpaceData*>(space, tuningSpaceData));
		}

		// Tuning Space名
		wstring temp;
		if (space == 0)
			temp = sTempTuningSpaceName;
		else
			temp = L"NoName";
		::GetPrivateProfileStringW(sectionname, L"TuningSpaceName", temp.c_str(), buf, 64, m_szIniFilePath);
		itSpace->second->sTuningSpaceName = buf;

		// UHF/CATVのCH設定を自動生成する
		::GetPrivateProfileStringW(sectionname, L"ChannelSettingsAuto", L"", buf, 256, m_szIniFilePath);
		temp = buf;
		if (temp == L"UHF") {
			for (unsigned int ch = 0; ch < 50; ch++) {
				itCh = itSpace->second->Channels.find(ch);
				if (itCh == itSpace->second->Channels.end()) {
					ChData *chData = new ChData();
					itCh = itSpace->second->Channels.insert(itSpace->second->Channels.begin(), pair<unsigned int, ChData*>(ch, chData));
				}

				itCh->second->Satellite = 0;
				itCh->second->Polarisation = 0;
				itCh->second->ModulationType = 0;
				itCh->second->Frequency = 473000 + 6000 * ch;
				::swprintf_s(buf, 256, L"%02dch", ch + 13);
#ifdef UNICODE
				itCh->second->sServiceName = buf;
#else
				::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
				chData->sServiceName = charBuf;
#endif
			}
			itSpace->second->dwNumChannel = 50;
		}
		else if (temp == L"CATV") {
			for (unsigned int ch = 0; ch < 51; ch++) {
				itCh = itSpace->second->Channels.find(ch);
				if (itCh == itSpace->second->Channels.end()) {
					ChData *chData = new ChData();
					itCh = itSpace->second->Channels.insert(itSpace->second->Channels.begin(), pair<unsigned int, ChData*>(ch, chData));
				}

				itCh->second->Satellite = 0;
				itCh->second->Polarisation = 0;
				itCh->second->ModulationType = 0;
				long f;
				if (ch <= 22 - 13) {
					f = 111000 + 6000 * ch;
					if (ch == 22 - 13) {
						f += 2000;
					}
				}
				else {
					f = 225000 + 6000 * (ch - (23 - 13));
					if (ch >= 24 - 13 && ch <= 27 - 13) {
						f += 2000;
					}
				}
				itCh->second->Frequency = f;
				::swprintf_s(buf, 256, L"C%02dch", ch + 13);
#ifdef UNICODE
				itCh->second->sServiceName = buf;
#else
				::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
				chData->sServiceName = charBuf;
#endif
			}
			itSpace->second->dwNumChannel = 51;
		}

		// CH設定
		//    チャンネル番号 = 衛星番号,周波数,偏波,変調方式[,チャンネル名[,SID/MinorChannel[,TSID/Channel[,ONID/PhysicalChannel[,MajorChannel[,SourceID]]]]]]
		//    例: CH001 = 1,12658,V,0
		//      チャンネル番号 : CH000～CH999で指定
		//      衛星番号       : Satteliteセクションで設定した衛星番号(1～4) または 0(未指定時)
		//                       (地デジチューナー等は0を指定してください)
		//      周波数         : 周波数をMHzで指定
		//                       (小数点を付けることによりKHz単位での指定が可能です)
		//      偏波           : 'V' = 垂直偏波 'H'=水平偏波 'L'=左旋円偏波 'R'=右旋円偏波 ' '=未指定
		//                       (地デジチューナー等は未指定)
		//      変調方式       : Modulationセクションで設定した変調方式番号(0～3)
		//      チャンネル名   : チャンネル名称
		//                       (省略した場合は 128.0E / 12658H / DVB - S のような形式で自動生成されます)
		//      SID            : DVB / ISDBのサービスID
		//      TSID           : DVB / ISDBのトランスポートストリームID
		//      ONID           : DVB / ISDBのオリジナルネットワークID
		//      MinorChannel   : ATSC / Digital CableのMinor Channel
		//      Channel        : ATSC / Digital CableのChannel
		//      PhysicalChannel: ATSC / Digital CableのPhysical Channel
		//      MajorChannel   : Digital CableのMajor Channel
		//      SourceID       : Digital CableのSourceID
		for (DWORD ch = 0; ch < 1000; ch++) {
			WCHAR keyname[64];

			::swprintf_s(keyname, 64, L"CH%03d", ch);
			result = ::GetPrivateProfileStringW(sectionname, keyname, L"", buf, 256, m_szIniFilePath);
			if (result <= 0)
				continue;

			// 設定行が有った
			// ReserveUnusedChが指定されている場合はCH番号を上書きする
			DWORD chNum = m_bReserveUnusedCh ? ch : (DWORD)(itSpace->second->Channels.size());
			itCh = itSpace->second->Channels.find(chNum);
			if (itCh == itSpace->second->Channels.end()) {
				ChData *chData = new ChData();
				itCh = itSpace->second->Channels.insert(itSpace->second->Channels.begin(), pair<unsigned int, ChData*>(chNum, chData));
			}

			WCHAR szSatellite[256] = L"";
			WCHAR szFrequency[256] = L"";
			WCHAR szPolarisation[256] = L"";
			WCHAR szModulationType[256] = L"";
			WCHAR szServiceName[256] = L"";
			WCHAR szSID[256] = L"";
			WCHAR szTSID[256] = L"";
			WCHAR szONID[256] = L"";
			WCHAR szMajorChannel[256] = L"";
			WCHAR szSourceID[256] = L"";
			::swscanf_s(buf, L"%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,]", szSatellite, 256, szFrequency, 256,
				szPolarisation, 256, szModulationType, 256, szServiceName, 256, szSID, 256, szTSID, 256, szONID, 256, szMajorChannel, 256, szSourceID, 256);

			// 衛星番号
			val = _wtoi(szSatellite);
			if (val >= 0 && val < MAX_SATELLITE) {
				itCh->second->Satellite = val;
			}
			else
				OutputDebug(L"Format Error in readIniFile; Wrong Bird.\n");

			// 周波数
			WCHAR szMHz[256] = L"";
			WCHAR szKHz[256] = L"";
			::swscanf_s(szFrequency, L"%[^.].%[^.]", szMHz, 256, szKHz, 256);
			val = _wtoi(szMHz) * 1000 + _wtoi(szKHz);
			if ((val > 0) && (val <= 20000000)) {
				itCh->second->Frequency = val;
			}
			else
				OutputDebug(L"Format Error in readIniFile; Wrong Frequency.\n");

			// 偏波種類
			if (szPolarisation[0] == L' ')
				szPolarisation[0] = L'\0';
			val = -1;
			for (unsigned int i = 0; i < POLARISATION_SIZE; i++) {
				if (szPolarisation[0] == PolarisationChar[i]) {
					val = i;
					break;
				}
			}
			if (val != -1) {
				itCh->second->Polarisation = val;
			}
			else
				OutputDebug(L"Format Error in readIniFile; Wrong Polarization.\n");

			// 変調方式
			val = _wtoi(szModulationType);
			if (val >= 0 && val < MAX_MODULATION) {
				itCh->second->ModulationType = val;
			}
			else
				OutputDebug(L"Format Error in readIniFile; Wrong Method.\n");

			// チャンネル名
			if (szServiceName[0] == 0)
				// iniファイルで指定した名称がなければ128.0E/12658H/DVB-S のような形式で作成する
				MakeChannelName(szServiceName, 256, itCh->second);

#ifdef UNICODE
			itCh->second->sServiceName = szServiceName;
#else
			::wcstombs_s(NULL, charBuf, 512, szServiceName, _TRUNCATE);
			chData->sServiceName = charBuf;
#endif

			// SID / PhysicalChannel
			if (szSID[0] != 0) {
				itCh->second->SID = wcstol(szSID, NULL, 0);
			}

			// TSID / Channel
			if (szTSID[0] != 0) {
				itCh->second->TSID = wcstol(szTSID, NULL, 0);
			}

			// ONID / MinorChannel
			if (szONID[0] != 0) {
				itCh->second->ONID = wcstol(szONID, NULL, 0);
			}

			// MajorChannel
			if (szMajorChannel[0] != 0) {
				itCh->second->MajorChannel = wcstol(szMajorChannel, NULL, 0);
			}

			// SourceID
			if (szSourceID[0] != 0) {
				itCh->second->SourceID = wcstol(szSourceID, NULL, 0);
			}
		}

		// CH番号の最大値 + 1
		itCh = itSpace->second->Channels.end();
		if (itCh == itSpace->second->Channels.begin()) {
			itSpace->second->dwNumChannel = 0;
		}
		else {
			itCh--;
			itSpace->second->dwNumChannel = itCh->first + 1;
		}

		// CH切替動作を強制的に2度行う場合の対象CH
		if (m_bLockTwice) {
			unsigned int len = ::GetPrivateProfileStringW(sectionname, L"ChannelLockTwiceTarget", L"", buf, 256, m_szIniFilePath);
			if (len > 0) {
				WCHAR szToken[256];
				unsigned int nPos = 0;
				int nTokenLen;
				while (nPos < len) {
					// カンマ区切りまでの文字列を取得
					::swscanf_s(&buf[nPos], L"%[^,]%n", szToken, 256, &nTokenLen);
					if (nTokenLen) {
						// さらに'-'区切りの数値に分解
						DWORD begin = 0;
						DWORD end = itSpace->second->dwNumChannel - 1;
						WCHAR s1[256] = L"";
						WCHAR s2[256] = L"";
						WCHAR s3[256] = L"";
						int num = ::swscanf_s(szToken, L" %[0-9] %[-] %[0-9]", s1, 256, s2, 256, s3, 256);
						switch (num)
						{
						case 1:
							// "10"の形式（単独指定）
							begin = end = _wtoi(s1);
							break;
						case 2:
							// "10-"の形式
							begin = _wtoi(s1);
							break;
						case 3:
							// "10-15"の形式
							begin = _wtoi(s1);
							end = _wtoi(s3);
							break;
						case 0:
							num = ::swscanf_s(szToken, L" %[-] %[0-9]", s2, 256, s3, 256);
							if (num == 2) {
								// "-10"の形式
								end = _wtoi(s3);
							}
							else {
								// 解析不能
								OutputDebug(L"Format Error in readIniFile; ChannelLockTwiceTarget.\n");
								continue;
							}
							break;
						}
						// 対象範囲のCHのFlagをSetする
						for (DWORD ch = begin; ch <= end; ch++) {
							itCh = itSpace->second->Channels.find(ch);
							if (itCh != itSpace->second->Channels.end()) {
								itCh->second->LockTwiceTarget = TRUE;
							}
						}
					} // if (nTokenLen)
					nPos += nTokenLen + 1;
				} // while (nPos < len)
			} // if (len > 0) 
			else {
				// ChannelLockTwiceTargetの指定が無い場合はすべてのCHが対象
				for (DWORD ch = 0; ch < itSpace->second->dwNumChannel - 1; ch++) {
					itCh = itSpace->second->Channels.find(ch);
					if (itCh != itSpace->second->Channels.end()) {
						itCh->second->LockTwiceTarget = TRUE;
					}
				}
			}
		} // if (m_bLockTwice)
	}

	// チューニング空間番号0を探す
	itSpace = m_TuningData.Spaces.find(0);
	if (itSpace == m_TuningData.Spaces.end()) {
		// ここには来ないはずだけど一応
		// 空のTuningSpaceDataをチューニング空間番号0に挿入
		TuningSpaceData *tuningSpaceData = new TuningSpaceData;
		itSpace = m_TuningData.Spaces.insert(m_TuningData.Spaces.begin(), pair<unsigned int, TuningSpaceData*>(0, tuningSpaceData));
	}

	if (!itSpace->second->Channels.size()) {
		// CH定義が一つもされていない
		if (m_nDefaultNetwork == 1) {
			// SPHDの場合のみ過去のバージョン互換動作
			// 3つのTPをデフォルトでセットしておく
			ChData *chData;
			//   128.0E 12.658GHz V DVB-S *** 2015-10-10現在、NITには存在するけど停波中
			chData = new ChData();
			chData->Satellite = 1;
			chData->Polarisation = 2;
			chData->ModulationType = 0;
			chData->Frequency = 12658000;
			MakeChannelName(buf, 256, chData);
#ifdef UNICODE
			chData->sServiceName = buf;
#else
			::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
			chData->sServiceName = charBuf;
#endif
			itSpace->second->Channels.insert(pair<unsigned int, ChData*>(0, chData));
			//   124.0E 12.613GHz H DVB-S2
			chData = new ChData();
			chData->Satellite = 2;
			chData->Polarisation = 1;
			chData->ModulationType = 1;
			chData->Frequency = 12613000;
			MakeChannelName(buf, 256, chData);
#ifdef UNICODE
			chData->sServiceName = buf;
#else
			::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
			chData->sServiceName = charBuf;
#endif
			itSpace->second->Channels.insert(pair<unsigned int, ChData*>(1, chData));
			//   128.0E 12.733GHz H DVB-S2
			chData = new ChData();
			chData->Satellite = 1;
			chData->Polarisation = 1;
			chData->ModulationType = 1;
			chData->Frequency = 12733000;
			MakeChannelName(buf, 256, chData);
#ifdef UNICODE
			chData->sServiceName = buf;
#else
			::wcstombs_s(NULL, charBuf, 512, buf, _TRUNCATE);
			chData->sServiceName = charBuf;
#endif
			itSpace->second->Channels.insert(pair<unsigned int, ChData*>(2, chData));
			itSpace->second->dwNumChannel = 3;
		}
	}

	// チューニング空間の数
	itSpace = m_TuningData.Spaces.end();
	if (itSpace == m_TuningData.Spaces.begin()) {
		// こっちも一応
		m_TuningData.dwNumSpace = 0;
	}
	else {
		itSpace--;
		m_TuningData.dwNumSpace = itSpace->first + 1;
	}
}

void CBonTuner::GetSignalState(int* pnStrength, int* pnQuality, int* pnLock)
{
	if (pnStrength) *pnStrength = 0;
	if (pnQuality) *pnQuality = 0;
	if (pnLock) *pnLock = 1;

	// チューナ固有 GetSignalState があれば、丸投げ
	HRESULT hr;
	if ((m_pIBdaSpecials) && (hr = m_pIBdaSpecials->GetSignalState(pnStrength, pnQuality, pnLock)) != E_NOINTERFACE) {
		// E_NOINTERFACE でなければ、固有関数があったという事なので、
		// このままリターン
		return;
	}

	if (m_pTunerDevice == NULL)
		return;

	long longVal;
	BYTE byteVal;

	if (m_pITuner) {
		if ((m_bSignalLevelGetTypeTuner && ((m_bSignalLevelNeedStrength && pnStrength) || (m_bSignalLevelNeedQuality && pnQuality))) ||
				(m_bSignalLockedJudgeTypeTuner && pnLock)) {
			longVal = 0;
			if (SUCCEEDED(hr = m_pITuner->get_SignalStrength(&longVal))) {
				int strength = (int)(longVal & 0xffff);
				int quality = (int)(longVal >> 16);
				if (m_bSignalLevelNeedStrength && pnStrength)
					*pnStrength = strength < 0 ? 0xffff - strength : strength;
				if (m_bSignalLevelNeedQuality && pnQuality)
					*pnQuality = min(max(quality, 0), 100);
				if (m_bSignalLockedJudgeTypeTuner && pnLock)
					*pnLock = strength > 0 ? 1 : 0;
			}
		}
	}

	if (m_pIBDA_SignalStatistics) {
		if (m_bSignalLevelGetTypeSS) {
			if (m_bSignalLevelNeedStrength && pnStrength) {
				longVal = 0;
				if (SUCCEEDED(hr = m_pIBDA_SignalStatistics->get_SignalStrength(&longVal)))
					*pnStrength = (int)(longVal & 0xffff);
			}

			if (m_bSignalLevelNeedQuality && pnQuality) {
				longVal = 0;
				if (SUCCEEDED(hr = m_pIBDA_SignalStatistics->get_SignalQuality(&longVal)))
					*pnQuality = (int)(min(max(longVal & 0xffff, 0), 100));
			}
		}

		if (m_bSignalLockedJudgeTypeSS && pnLock) {
			byteVal = 0;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatistics->get_SignalLocked(&byteVal)))
				*pnLock = (int)byteVal;
		}
	}

	return;
}

BOOL CBonTuner::LockChannel(const TuningParam *pTuningParam, BOOL bLockTwice)
{
	HRESULT hr;

	// チューナ固有 LockChannel があれば、丸投げ
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->LockChannel(pTuningParam)) != E_NOINTERFACE) {
		// BonDriver_BDA改専用DLL
		// E_NOINTERFACE でなければ、固有関数があったという事なので、
		// その中で選局処理が行なわれているはず。よってこのままリターン
		m_nCurTone = pTuningParam->Antenna->Tone;
		if (SUCCEEDED(hr) && bLockTwice) {
			OutputDebug(L"TwiceLock 1st[Special2] SUCCESS.\n");
			SleepWithMessageLoop(m_nLockTwiceDelay);
			hr = m_pIBdaSpecials2->LockChannel(pTuningParam);
		}
		if (SUCCEEDED(hr)) {
			OutputDebug(L"LockChannel[Special2] SUCCESS.\n");
			return TRUE;
		} else {
			OutputDebug(L"LockChannel[Special2] FAIL.\n");
			return FALSE;
		}
	}

	if (m_pIBdaSpecials && (hr = m_pIBdaSpecials->LockChannel(pTuningParam->Antenna->Tone ? 1 : 0, (pTuningParam->Polarisation == BDA_POLARISATION_LINEAR_H) ? TRUE : FALSE, pTuningParam->Frequency / 1000,
			(pTuningParam->Modulation->Modulation == BDA_MOD_NBC_8PSK || pTuningParam->Modulation->Modulation == BDA_MOD_8PSK) ? TRUE : FALSE)) != E_NOINTERFACE) {
		// BonDriver_BDAオリジナル互換DLL
		// E_NOINTERFACE でなければ、固有関数があったという事なので、
		// その中で選局処理が行なわれているはず。よってこのままリターン
		m_nCurTone = pTuningParam->Antenna->Tone;
		if (SUCCEEDED(hr) && bLockTwice) {
			OutputDebug(L"TwiceLock 1st[Special] SUCCESS.\n");
			SleepWithMessageLoop(m_nLockTwiceDelay);
			hr = m_pIBdaSpecials->LockChannel(pTuningParam->Antenna->Tone ? 1 : 0, (pTuningParam->Polarisation == BDA_POLARISATION_LINEAR_H) ? TRUE : FALSE, pTuningParam->Frequency / 1000,
					(pTuningParam->Modulation->Modulation == BDA_MOD_NBC_8PSK || pTuningParam->Modulation->Modulation == BDA_MOD_8PSK) ? TRUE : FALSE);
		}
		if (SUCCEEDED(hr)) {
			OutputDebug(L"LockChannel[Special] SUCCESS.\n");
			return TRUE;
		}
		else {
			OutputDebug(L"LockChannel[Special] FAIL.\n");
			return FALSE;
		}
	}

	// チューナ固有トーン制御関数があれば、それをここで呼び出す
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->Set22KHz(pTuningParam->Antenna->Tone)) != E_NOINTERFACE) {
		// BonDriver_BDA改専用DLL
		if (SUCCEEDED(hr)) {
			OutputDebug(L"Set22KHz[Special2] successfully.\n");
			if (pTuningParam->Antenna->Tone != m_nCurTone) {
				m_nCurTone = pTuningParam->Antenna->Tone;
				SleepWithMessageLoop(m_nToneWait); // 衛星切替待ち
			}
		}
		else {
			OutputDebug(L"Set22KHz[Special2] failed.\n");
			// BDA generic な方法で切り替わるかもしれないので、メッセージだけ出して、そのまま続行
		}
	}
	else if (m_pIBdaSpecials && (hr = m_pIBdaSpecials->Set22KHz(pTuningParam->Antenna->Tone ? 1 : 0)) != E_NOINTERFACE) {
		// BonDriver_BDAオリジナル互換DLL
		if (SUCCEEDED(hr)) {
			OutputDebug(L"Set22KHz[Special] successfully.\n");
			if (pTuningParam->Antenna->Tone != m_nCurTone) {
				m_nCurTone = pTuningParam->Antenna->Tone;
				SleepWithMessageLoop(m_nToneWait); // 衛星切替待ち
			}
		}
		else {
			OutputDebug(L"Set22KHz[Special] failed.\n");
			// BDA generic な方法で切り替わるかもしれないので、メッセージだけ出して、そのまま続行
		}
	}
	else {
		// 固有関数がないだけなので、何もせず
	}

	// IDVBSTuningSpace特有
	{
		CComQIPtr<IDVBSTuningSpace> pIDVBSTuningSpace(m_pITuningSpace);
		if (pIDVBSTuningSpace) {
			// LNB 周波数を設定
			if (pTuningParam->Antenna->HighOscillator != -1) {
				pIDVBSTuningSpace->put_HighOscillator(pTuningParam->Antenna->HighOscillator);
			}
			if (pTuningParam->Antenna->LowOscillator != -1) {
				pIDVBSTuningSpace->put_LowOscillator(pTuningParam->Antenna->LowOscillator);
			}

			// LNBスイッチの周波数を設定
			if (pTuningParam->Antenna->LNBSwitch != -1) {
				// LNBSwitch周波数の設定がされている
				pIDVBSTuningSpace->put_LNBSwitch(pTuningParam->Antenna->LNBSwitch);
			}
			else {
				// 10GHzを設定しておけばHigh側に、20GHzを設定しておけばLow側に切替わるはず
				pIDVBSTuningSpace->put_LNBSwitch((pTuningParam->Antenna->Tone != 0) ? 10000000 : 20000000);
			}

			// 位相変調スペクトル反転の種類
			pIDVBSTuningSpace->put_SpectralInversion(BDA_SPECTRAL_INVERSION_AUTOMATIC);
		}
	}

	// ILocator取得
	CComPtr<ILocator> pILocator;
	if (FAILED(hr = m_pITuningSpace->get_DefaultLocator(&pILocator)) || !pILocator) {
		OutputDebug(L"Fail to get ILocator.\n");
		return FALSE;
	}

	// RF 信号の周波数を設定
	pILocator->put_CarrierFrequency(pTuningParam->Frequency);

	// 内部前方誤り訂正のタイプを設定
	pILocator->put_InnerFEC(pTuningParam->Modulation->InnerFEC);

	// 内部 FEC レートを設定
	// 前方誤り訂正方式で使うバイナリ コンボルーションのコード レート DVB-Sは 3/4 S2は 3/5
	pILocator->put_InnerFECRate(pTuningParam->Modulation->InnerFECRate);

	// 変調タイプを設定
	// DVB-SはQPSK、S2の場合は 8PSK
	pILocator->put_Modulation(pTuningParam->Modulation->Modulation);

	// 外部前方誤り訂正のタイプを設定
	//	リード-ソロモン 204/188 (外部 FEC), DVB-S2でも同じ
	pILocator->put_OuterFEC(pTuningParam->Modulation->OuterFEC);

	// 外部 FEC レートを設定
	pILocator->put_OuterFECRate(pTuningParam->Modulation->OuterFECRate);

	// QPSK シンボル レートを設定
	pILocator->put_SymbolRate(pTuningParam->Modulation->SymbolRate);

	// IDVBSLocator特有
	{
		CComQIPtr<IDVBSLocator> pIDVBSLocator(pILocator);
		if (pIDVBSLocator) {
			// 信号の偏波を設定
			pIDVBSLocator->put_SignalPolarisation(pTuningParam->Polarisation);
		}
	}

	// IDVBSLocator2特有
	{
		CComQIPtr<IDVBSLocator2> pIDVBSLocator2(pILocator);
		if (pIDVBSLocator2) {
			// DiSEqCを設定
			if (pTuningParam->Antenna->DiSEqC >= BDA_LNB_SOURCE_A) {
				pIDVBSLocator2->put_DiseqLNBSource((LNB_Source)(pTuningParam->Antenna->DiSEqC));
			}
		}
	}

	// IDVBTLocator特有
	{
		CComQIPtr<IDVBTLocator> pIDVBTLocator(pILocator);
		if (pIDVBTLocator) {
			// 周波数の帯域幅 (MHz)を設定
			if (pTuningParam->Modulation->BandWidth != -1) {
				pIDVBTLocator->put_Bandwidth(pTuningParam->Modulation->BandWidth);
			}
		}
	}

	// IATSCLocator特有
	{
		CComQIPtr<IATSCLocator> pIATSCLocator(pILocator);
		if (pIATSCLocator) {
			// ATSC PhysicalChannel
			if (pTuningParam->PhysicalChannel != -1) {
				pIATSCLocator->put_PhysicalChannel(pTuningParam->PhysicalChannel);
			}
		}
	}

	// ITuneRequest作成
	CComPtr<ITuneRequest> pITuneRequest;
	if (FAILED(hr = m_pITuningSpace->CreateTuneRequest(&pITuneRequest))) {
		OutputDebug(L"Fail to create ITuneRequest.\n");
		return FALSE;
	}

	// ITuneRequestにILocatorを設定
	hr = pITuneRequest->put_Locator(pILocator);

	// IDVBTuneRequest特有
	{
		CComQIPtr<IDVBTuneRequest> pIDVBTuneRequest(pITuneRequest);
		if (pIDVBTuneRequest) {
			// DVB Triplet IDの設定
			pIDVBTuneRequest->put_ONID(pTuningParam->ONID);
			pIDVBTuneRequest->put_TSID(pTuningParam->TSID);
			pIDVBTuneRequest->put_SID(pTuningParam->SID);
		}
	}

	// IChannelTuneRequest特有
	{
		CComQIPtr<IChannelTuneRequest> pIChannelTuneRequest(pITuneRequest);
		if (pIChannelTuneRequest) {
			// ATSC Channel
			pIChannelTuneRequest->put_Channel(pTuningParam->Channel);
		}
	}

	// IATSCChannelTuneRequest特有
	{
		CComQIPtr<IATSCChannelTuneRequest> pIATSCChannelTuneRequest(pITuneRequest);
		if (pIATSCChannelTuneRequest) {
			// ATSC MinorChannel
			pIATSCChannelTuneRequest->put_MinorChannel(pTuningParam->MinorChannel);
		}
	}

	// IDigitalCableTuneRequest特有
	{
		CComQIPtr<IDigitalCableTuneRequest> pIDigitalCableTuneRequest(pITuneRequest);
		if (pIDigitalCableTuneRequest) {
			// Digital Cable MinorChannel
			pIDigitalCableTuneRequest->put_MajorChannel(pTuningParam->MinorChannel);
			// Digital Cable SourceID
			pIDigitalCableTuneRequest->put_SourceID(pTuningParam->SourceID);
		}
	}

	if (m_pIBdaSpecials2) {
		// m_pIBdaSpecialsでput_TuneRequestの前に何らかの処理が必要なら行う
		hr = m_pIBdaSpecials2->PreTuneRequest(pTuningParam, pITuneRequest);
	}

	if (pTuningParam->Antenna->Tone != m_nCurTone) {
		//トーン切替ありの場合、先に一度TuneRequestしておく
		OutputDebug(L"Requesting pre tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"Fail to put pre tune request.\n");
			return FALSE;
		}
		OutputDebug(L"Pre tune request complete.\n");

		m_nCurTone = pTuningParam->Antenna->Tone;
		SleepWithMessageLoop(m_nToneWait); // 衛星切替待ち
	}

	if (bLockTwice) {
		// TuneRequestを強制的に2度行う
		OutputDebug(L"Requesting 1st twice tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"Fail to put 1st twice tune request.\n");
			return FALSE;
		}
		OutputDebug(L"1st Twice tune request complete.\n");
		SleepWithMessageLoop(m_nLockTwiceDelay);
	}

	unsigned int nRetryRemain = m_nLockWaitRetry;
	int nLock = 0;
	do {
		OutputDebug(L"Requesting tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"Fail to put tune request.\n");
			return FALSE;
		}
		OutputDebug(L"Tune request complete.\n");

		static const int LockRetryTime = 50;
		unsigned int nWaitRemain = m_nLockWait;
		SleepWithMessageLoop(m_nLockWaitDelay);
		GetSignalState(NULL, NULL, &nLock);
		while (!nLock && nWaitRemain) {
			DWORD dwSleepTime = (nWaitRemain > LockRetryTime) ? LockRetryTime : nWaitRemain;
			OutputDebug(L"Waiting lock status after %d msec.\n", nWaitRemain);
			SleepWithMessageLoop(dwSleepTime);
			nWaitRemain -= dwSleepTime;
			GetSignalState(NULL, NULL, &nLock);
		}
	} while (!nLock && nRetryRemain--);

	if (nLock != 0)
		OutputDebug(L"LockChannel success.\n");
	else
		OutputDebug(L"LockChannel failed.\n");

	return nLock != 0;
}

// チューナ固有Dllのロード
HRESULT CBonTuner::CheckAndInitTunerDependDll(wstring tunerGUID, wstring tunerFriendlyName)
{
	if (m_aTunerParam.sDLLBaseName == L"") {
		// チューナ固有関数を使わない場合
		return S_OK;
	}

	if (m_aTunerParam.sDLLBaseName == L"AUTO") {
		// INI ファイルで "AUTO" 指定の場合
		BOOL found = FALSE;
		for (unsigned int i = 0; i < sizeof aTunerSpecialData / sizeof TUNER_SPECIAL_DLL; i++) {
			if ((aTunerSpecialData[i].sTunerGUID != L"") && (tunerGUID.find(aTunerSpecialData[i].sTunerGUID)) != wstring::npos) {
				// この時のチューナ依存コードをチューナパラメータに変数にセットする
				m_aTunerParam.sDLLBaseName = aTunerSpecialData[i].sDLLBaseName;
				break;
			}
		}
		if (!found) {
			// 見つからなかったのでチューナ固有関数は使わない
			return S_OK;
		}
	}

	// ここで DLL をロードする。
	WCHAR szPath[_MAX_PATH + 1] = L"";
	::GetModuleFileNameW(st_hModule, szPath, _MAX_PATH + 1);
	// フルパスを分解
	WCHAR szDrive[_MAX_DRIVE];
	WCHAR szDir[_MAX_DIR];
	WCHAR szFName[_MAX_FNAME];
	WCHAR szExt[_MAX_EXT];
	::_wsplitpath_s(szPath, szDrive, szDir, szFName, szExt);

	// フォルダ名取得
	WCHAR szDLLName[_MAX_PATH + 1];
	::swprintf_s(szDLLName, _MAX_PATH + 1, L"%s%s%s.dll", szDrive, szDir, m_aTunerParam.sDLLBaseName.c_str());

	if ((m_hModuleTunerSpecials = ::LoadLibraryW(szDLLName)) == NULL) {
		// ロードできない場合、どうする? 
		//  → デバッグメッセージだけ出して、固有関数を使わないものとして扱う
		OutputDebug(L"DLL Not found.\n");
		return S_OK;
	} else {
		OutputDebug(L"Load Library successfully.\n");
	}

	HRESULT(*func)(IBaseFilter*, const WCHAR*, const WCHAR*, const WCHAR*) =
		(HRESULT(*)(IBaseFilter*, const WCHAR*, const WCHAR*, const WCHAR*))::GetProcAddress(m_hModuleTunerSpecials, "CheckAndInitTuner");
	if (!func) {
		// 初期化コードが無い
		// →初期化不要
		return S_OK;
	}

	return (*func)(m_pTunerDevice, tunerGUID.c_str(), tunerFriendlyName.c_str(), m_szIniFilePath);
}

// チューナ固有Dllでのキャプチャデバイス確認
HRESULT CBonTuner::CheckCapture(wstring tunerGUID, wstring tunerFriendlyName, wstring captureGUID, wstring captureFriendlyName)
{
	if (m_hModuleTunerSpecials == NULL) {
		return S_OK;
	}

	HRESULT(*func)(const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*) =
		(HRESULT(*)(const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*))::GetProcAddress(m_hModuleTunerSpecials, "CheckCapture");
	if (!func) {
		return S_OK;
	}

	return (*func)(tunerGUID.c_str(), tunerFriendlyName.c_str(), captureGUID.c_str(), captureFriendlyName.c_str(), m_szIniFilePath);
}

// チューナ固有関数のロード
void CBonTuner::LoadTunerDependCode(void)
{
	if (!m_hModuleTunerSpecials)
		return;

	IBdaSpecials* (*func)(CComPtr<IBaseFilter>);
	func = (IBdaSpecials* (*)(CComPtr<IBaseFilter>))::GetProcAddress(m_hModuleTunerSpecials, "CreateBdaSpecials");
	if (!func) {
		OutputDebug(L"Cannot find CreateBdaSpecials.\n");
		::FreeLibrary(m_hModuleTunerSpecials);
		m_hModuleTunerSpecials = NULL;
		return;
	}
	else {
		OutputDebug(L"CreateBdaSpecials found.\n");
	}

	m_pIBdaSpecials = func(m_pTunerDevice);

	m_pIBdaSpecials2 = dynamic_cast<IBdaSpecials2a1 *>(m_pIBdaSpecials);
	if (!m_pIBdaSpecials2)
		OutputDebug(L"Not IBdaSpecials2 Interface DLL.\n");

	//  BdaSpecialsにiniファイルを読み込ませる
	HRESULT hr;
	if (m_pIBdaSpecials2) {
		hr = m_pIBdaSpecials2->ReadIniFile(m_szIniFilePath);
	}

	// チューナ固有初期化関数をここで実行しておく
	if (m_pIBdaSpecials)
		m_pIBdaSpecials->InitializeHook();

	return;
}

// チューナ固有関数とDllの解放
void CBonTuner::ReleaseTunerDependCode(void)
{
	HRESULT hr;

	// チューナ固有関数が定義されていれば、ここで実行しておく
	if (m_pIBdaSpecials) {
		if ((hr = m_pIBdaSpecials->FinalizeHook()) == E_NOINTERFACE) {
			// 固有Finalize関数がないだけなので、何もせず
		}
		else if (SUCCEEDED(hr)) {
			OutputDebug(L"Tuner Special Finalize successfully.\n");
		}
		else {
			OutputDebug(L"Tuner Special Finalize failed.\n");
		}

		SAFE_RELEASE(m_pIBdaSpecials);
		m_pIBdaSpecials2 = NULL;
	}

	if (m_hModuleTunerSpecials) {
		if (::FreeLibrary(m_hModuleTunerSpecials) == 0) {
			OutputDebug(L"FreeLibrary failed.\n");
		}
		else {
			OutputDebug(L"FreeLibrary Success.\n");
			m_hModuleTunerSpecials = NULL;
		}
	}
}

HRESULT CBonTuner::InitializeGraphBuilder(void)
{
	HRESULT hr;
	if (FAILED(hr = ::CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&m_pIGraphBuilder))) {
		OutputDebug(L"Fail to create Graph.\n");
		return hr;
	}

	if (FAILED(hr = m_pIGraphBuilder->QueryInterface(&m_pIMediaControl))) {
		OutputDebug(L"Fail to get IMediaControl.\n");
		return hr;
	}

	return S_OK;
}

void CBonTuner::CleanupGraph(void)
{
	DisconnectAll(m_pTif);
	DisconnectAll(m_pDemux);
	DisconnectAll(m_pTsWriter);
	DisconnectAll(m_pCaptureDevice);
	DisconnectAll(m_pTunerDevice);
	DisconnectAll(m_pNetworkProvider);

	UnloadTif();
	UnloadDemux();
	UnloadTsWriter();

	// Tuner → Capture の順で Release しないと
	// メモリリークを起こすデバイスがある
	UnloadTunerDevice();
	UnloadCaptureDevice();

	UnloadNetworkProvider();
	UnloadTuningSpace();

	SAFE_RELEASE(m_pIMediaControl);
	SAFE_RELEASE(m_pIGraphBuilder);

	return;
}

HRESULT CBonTuner::RunGraph(void)
{
	HRESULT hr;
	if (!m_pIMediaControl)
		return E_POINTER;

	if (FAILED(hr =  m_pIMediaControl->Run())) {
		m_pIMediaControl->Stop();
		OutputDebug(L"Failed to Run Graph.\n");
		return hr;
	}

	return S_OK;
}

void CBonTuner::StopGraph(void)
{
	HRESULT hr;
	if (m_pIMediaControl) {
		if (SUCCEEDED(hr = m_pIMediaControl->Pause())) {
			OutputDebug(L"IMediaControl::Pause Success.\n");
		} else {
			OutputDebug(L"IMediaControl::Pause failed.\n");
		}

		if (SUCCEEDED(hr = m_pIMediaControl->Stop())) {
			OutputDebug(L"IMediaControl::Stop Success.\n");
		} else {
			OutputDebug(L"IMediaControl::Stop failed.\n");
		}
	}
}

HRESULT CBonTuner::CreateTuningSpace(void)
{
	CLSID clsidTuningSpace = CLSID_NULL;
	IID iidITuningSpace = IID_NULL;
	CLSID clsidLocator = CLSID_NULL;
	IID iidNetworkType = IID_NULL;
	ModulationType modulationType = BDA_MOD_NOT_SET;
	_bstr_t bstrUniqueName;
	_bstr_t bstrFriendlyName;
	DVBSystemType dvbSystemType = DVB_Satellite;
	long networkID = -1;
	long highOscillator = -1;
	long lowOscillator = -1;
	long lnbSwitch = -1;
	TunerInputType tunerInputType = TunerInputCable;
	long minChannel = 0;
	long maxChannel = 0;
	long minPhysicalChannel = 0;
	long maxPhysicalChannel = 0;
	long minMinorChannel = 0;
	long maxMinorChannel = 0;
	long minMajorChannel = 0;
	long maxMajorChannel = 0;
	long minSourceID = 0;
	long maxSourceID = 0;

	switch (m_nDVBSystemType) {
	case eTunerTypeDVBT:
	case eTunerTypeDVBT2:
		bstrUniqueName = L"DVB-T";
		bstrFriendlyName = L"Local DVB-T Digital Antenna";
		clsidTuningSpace = __uuidof(DVBTuningSpace);
		iidITuningSpace = __uuidof(IDVBTuningSpace2);
		if (m_nDVBSystemType == eTunerTypeDVBT2) {
			clsidLocator = __uuidof(DVBTLocator2);
		}
		else {
			clsidLocator = __uuidof(DVBTLocator);
		}
		iidNetworkType = { STATIC_DVB_TERRESTRIAL_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		dvbSystemType = DVB_Terrestrial;
		networkID = 0;
		break;

	case eTunerTypeDVBC:
		bstrUniqueName = L"DVB-C";
		bstrFriendlyName = L"Local DVB-C Digital Cable";
		clsidTuningSpace = __uuidof(DVBTuningSpace);
		iidITuningSpace = __uuidof(IDVBTuningSpace2);
		clsidLocator = __uuidof(DVBCLocator);
		iidNetworkType = { STATIC_DVB_CABLE_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		dvbSystemType = DVB_Cable;
		networkID = 0;
		break;

	case eTunerTypeISDBT:
		bstrUniqueName = L"ISDB-T";
		bstrFriendlyName = L"Local ISDB-T Digital Antenna";
		clsidTuningSpace = __uuidof(DVBTuningSpace);
		iidITuningSpace = __uuidof(IDVBTuningSpace2);
		clsidLocator = __uuidof(DVBTLocator);
		iidNetworkType = { STATIC_ISDB_TERRESTRIAL_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		dvbSystemType = ISDB_Terrestrial;
		networkID = -1;
		break;

	case eTunerTypeISDBS:
		bstrUniqueName = L"ISDB-S";
		bstrFriendlyName = L"Default Digital ISDB-S Tuning Space";
		clsidTuningSpace = __uuidof(DVBSTuningSpace);
		iidITuningSpace = __uuidof(IDVBSTuningSpace);
		clsidLocator = __uuidof(DVBSLocator);
		iidNetworkType = { STATIC_ISDB_SATELLITE_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		dvbSystemType = ISDB_Satellite;
		networkID = -1;
		highOscillator = -1;
		lowOscillator = -1;
		lnbSwitch = -1;
		break;

	case eTunerTypeATSC_Antenna:
		bstrUniqueName = L"ATSC";
		bstrFriendlyName = L"Local ATSC Digital Antenna";
		clsidTuningSpace = __uuidof(ATSCTuningSpace);
		iidITuningSpace = __uuidof(IATSCTuningSpace);
		clsidLocator = __uuidof(ATSCLocator);
		iidNetworkType = { STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_128QAM;
		tunerInputType = TunerInputAntenna;
		minChannel = 1;
		maxChannel = 99;
		minPhysicalChannel = 2;
		maxPhysicalChannel = 158;
		minMinorChannel = 0;
		minMinorChannel = 999;
		break;

	case eTunerTypeATSC_Cable:
		bstrUniqueName = L"ATSCCable";
		bstrFriendlyName = L"Local ATSC Digital Cable";
		clsidTuningSpace = __uuidof(ATSCTuningSpace);
		iidITuningSpace = __uuidof(IATSCTuningSpace);
		clsidLocator = __uuidof(ATSCLocator);
		iidNetworkType = { STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_128QAM;
		tunerInputType = TunerInputCable;
		minChannel = 1;
		maxChannel = 99;
		minPhysicalChannel = 1;
		maxPhysicalChannel = 158;
		minMinorChannel = 0;
		minMinorChannel = 999;
		break;

	case eTunerTypeDigitalCable:
		bstrUniqueName = L"Digital Cable";
		bstrFriendlyName = L"Local Digital Cable";
		clsidTuningSpace = __uuidof(DigitalCableTuningSpace);
		iidITuningSpace = __uuidof(IDigitalCableTuningSpace);
		clsidLocator = __uuidof(DigitalCableLocator);
		iidNetworkType = { STATIC_DIGITAL_CABLE_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		tunerInputType = TunerInputCable;
		minChannel = 2;
		maxChannel = 9999;
		minPhysicalChannel = 2;
		maxPhysicalChannel = 158;
		minMinorChannel = 0;
		minMinorChannel = 999;
		minMajorChannel = 1;
		minMajorChannel = 99;
		minSourceID = 0;
		maxSourceID = 0x7fffffff;
		break;

	case eTunerTypeDVBS:
	default:
		bstrUniqueName = L"DVB-S";
		bstrFriendlyName = L"Default Digital DVB-S Tuning Space";
		clsidTuningSpace = __uuidof(DVBSTuningSpace);
		iidITuningSpace = __uuidof(IDVBSTuningSpace);
		clsidLocator = __uuidof(DVBSLocator);
		iidNetworkType = { STATIC_DVB_SATELLITE_TV_NETWORK_TYPE };
		modulationType = BDA_MOD_NOT_SET;
		dvbSystemType = DVB_Satellite;
		networkID = -1;
		highOscillator = 10600000;
		lowOscillator = 9750000;
		lnbSwitch = 11700000;
		break;
	}

	HRESULT hr;

	// ITuningSpaceを作成
	//
	// ITuningSpace継承順：
	//   ITuningSpace → IDVBTuningSpace → IDVBTuningSpace2 → IDVBSTuningSpace
	//                → IAnalogTVTuningSpace → IATSCTuningSpace → IDigitalCableTuningSpace
	if (FAILED(hr = ::CoCreateInstance(clsidTuningSpace, NULL, CLSCTX_INPROC_SERVER, iidITuningSpace, (void**)&m_pITuningSpace))) {
		OutputDebug(L"FAILED: CoCreateInstance(ITuningSpace)\n");
		return hr;
	}
	if (!m_pITuningSpace) {
		OutputDebug(L"Failed to get DVBSTuningSpace\n");
		return E_FAIL;
	}

	// ITuningSpace
	if (FAILED(hr = m_pITuningSpace->put__NetworkType(iidNetworkType))) {
		OutputDebug(L"put_NetworkType failed\n");
		return hr;
	}
	m_pITuningSpace->put_FrequencyMapping(L"");
	m_pITuningSpace->put_UniqueName(bstrUniqueName);
	m_pITuningSpace->put_FriendlyName(bstrFriendlyName);

	// IDVBTuningSpace特有
	{
		CComQIPtr<IDVBTuningSpace> pIDVBTuningSpace(m_pITuningSpace);
		if (pIDVBTuningSpace) {
			pIDVBTuningSpace->put_SystemType(dvbSystemType);
		}
	}

	// IDVBTuningSpace2特有
	{
		CComQIPtr<IDVBTuningSpace2> pIDVBTuningSpace2(m_pITuningSpace);
		if (pIDVBTuningSpace2) {
			pIDVBTuningSpace2->put_NetworkID(networkID);
		}
	}

	// IDVBSTuningSpace特有
	{
		CComQIPtr<IDVBSTuningSpace> pIDVBSTuningSpace(m_pITuningSpace);
		if (pIDVBSTuningSpace) {
			pIDVBSTuningSpace->put_HighOscillator(highOscillator);
			pIDVBSTuningSpace->put_LowOscillator(lowOscillator);
			pIDVBSTuningSpace->put_LNBSwitch(lnbSwitch);
			pIDVBSTuningSpace->put_SpectralInversion(BDA_SPECTRAL_INVERSION_NOT_SET);
		}
	}

	// IAnalogTVTuningSpace特有
	{
		CComQIPtr<IAnalogTVTuningSpace> pIAnalogTVTuningSpace(m_pITuningSpace);
		if (pIAnalogTVTuningSpace) {
			pIAnalogTVTuningSpace->put_InputType(tunerInputType);
			pIAnalogTVTuningSpace->put_MinChannel(minChannel);
			pIAnalogTVTuningSpace->put_MaxChannel(maxChannel);
			pIAnalogTVTuningSpace->put_CountryCode(0);
		}
	}

	// IATSCTuningSpace特有
	{
		CComQIPtr<IATSCTuningSpace> pIATSCTuningSpace(m_pITuningSpace);
		if (pIATSCTuningSpace) {
			pIATSCTuningSpace->put_MinPhysicalChannel(minPhysicalChannel);
			pIATSCTuningSpace->put_MaxPhysicalChannel(maxPhysicalChannel);
			pIATSCTuningSpace->put_MinMinorChannel(minMinorChannel);
			pIATSCTuningSpace->put_MaxMinorChannel(maxMinorChannel);
		}
	}

	// IDigitalCableTuningSpace特有
	{
		CComQIPtr<IDigitalCableTuningSpace> pIDigitalCableTuningSpace(m_pITuningSpace);
		if (pIDigitalCableTuningSpace) {
			pIDigitalCableTuningSpace->put_MinMajorChannel(minMajorChannel);
			pIDigitalCableTuningSpace->put_MaxMajorChannel(maxMajorChannel);
			pIDigitalCableTuningSpace->put_MinSourceID(minSourceID);
			pIDigitalCableTuningSpace->put_MaxSourceID(maxSourceID);
		}
	}

	// pILocatorを作成
	//
	// ILocator継承順：
	//   ILocator → IDVBTLocator → IDVBTLocator2
	//            → IDVBSLocator → IDVBSLocator2
	//            → IDVBCLocator
	//            → IDigitalLocator → IATSCLocator → IATSCLocator2 → IDigitalCableLocator
	CComPtr<ILocator> pILocator;
	if (FAILED(hr = pILocator.CoCreateInstance(clsidLocator)) || !pILocator) {
		OutputDebug(L"Fail to get ILocator.\n");
		return FALSE;
	}

	// ILocator
	pILocator->put_CarrierFrequency(-1);
	pILocator->put_SymbolRate(-1);
	pILocator->put_InnerFEC(BDA_FEC_METHOD_NOT_SET);
	pILocator->put_InnerFECRate(BDA_BCC_RATE_NOT_SET);
	pILocator->put_OuterFEC(BDA_FEC_METHOD_NOT_SET);
	pILocator->put_OuterFECRate(BDA_BCC_RATE_NOT_SET);
	pILocator->put_Modulation(modulationType);

	// IDVBSLocator特有
	{
		CComQIPtr<IDVBSLocator> pIDVBSLocator(pILocator);
		if (pIDVBSLocator) {
			pIDVBSLocator->put_WestPosition(FALSE);
			pIDVBSLocator->put_OrbitalPosition(-1);
			pIDVBSLocator->put_Elevation(-1);
			pIDVBSLocator->put_Azimuth(-1);
			pIDVBSLocator->put_SignalPolarisation(BDA_POLARISATION_NOT_SET);
		}
	}

	// IDVBSLocator2特有
	{
		CComQIPtr<IDVBSLocator2> pIDVBSLocator2(pILocator);
		if (pIDVBSLocator2) {
			pIDVBSLocator2->put_LocalOscillatorOverrideHigh(-1);
			pIDVBSLocator2->put_LocalOscillatorOverrideLow(-1);
			pIDVBSLocator2->put_LocalLNBSwitchOverride(-1);
			pIDVBSLocator2->put_LocalSpectralInversionOverride(BDA_SPECTRAL_INVERSION_NOT_SET);
			pIDVBSLocator2->put_DiseqLNBSource(BDA_LNB_SOURCE_NOT_SET);
			pIDVBSLocator2->put_SignalPilot(BDA_PILOT_NOT_SET);
			pIDVBSLocator2->put_SignalRollOff(BDA_ROLL_OFF_NOT_SET);
		}
	}

	// IDVBTLocator特有
	{
		CComQIPtr<IDVBTLocator> pIDVBTLocator(pILocator);
		if (pIDVBTLocator) {
			pIDVBTLocator->put_Bandwidth(-1);
			pIDVBTLocator->put_Guard(BDA_GUARD_NOT_SET);
			pIDVBTLocator->put_HAlpha(BDA_HALPHA_NOT_SET);
			pIDVBTLocator->put_LPInnerFEC(BDA_FEC_METHOD_NOT_SET);
			pIDVBTLocator->put_LPInnerFECRate(BDA_BCC_RATE_NOT_SET);
			pIDVBTLocator->put_Mode(BDA_XMIT_MODE_NOT_SET);
			pIDVBTLocator->put_OtherFrequencyInUse(VARIANT_FALSE);
		}
	}

	// IDVBTLocator2特有
	{
		CComQIPtr<IDVBTLocator2> pIDVBTLocator2(pILocator);
		if (pIDVBTLocator2) {
			pIDVBTLocator2->put_PhysicalLayerPipeId(-1);
		}
	}

	// IATSCLocator特有
	{
		CComQIPtr<IATSCLocator> pIATSCLocator(pILocator);
		if (pIATSCLocator) {
			pIATSCLocator->put_PhysicalChannel(-1);
			pIATSCLocator->put_TSID(-1);
		}
	}

	// IATSCLocator2特有
	{
		CComQIPtr<IATSCLocator2> pIATSCLocator2(pILocator);
		if (pIATSCLocator2) {
			pIATSCLocator2->put_ProgramNumber(-1);
		}
	}

	m_pITuningSpace->put_DefaultLocator(pILocator);

	return S_OK;
}

void CBonTuner::UnloadTuningSpace(void)
{
	SAFE_RELEASE(m_pITuningSpace);
}

// Tuning Request を送って Tuning Space を初期化する
//   これをやらないと output pin が出現しないチューナフィルタが
//   あるらしい
HRESULT CBonTuner::InitTuningSpace(void)
{
	if (!m_pITuningSpace) {
		OutputDebug(L"TuningSpace NOT SET.\n");
		return E_POINTER;
	}

	if (!m_pITuner) {
		OutputDebug(L"ITuner NOT SET.\n");
		return E_POINTER;
	}

	HRESULT hr;

	// ITuneRequest作成
	CComPtr<ITuneRequest> pITuneRequest;
	if (FAILED(hr = m_pITuningSpace->CreateTuneRequest(&pITuneRequest))) {
		OutputDebug(L"Fail to create ITuneRequest.\n");
		return hr;
	}

	// IDVBTuneRequest特有
	{
		CComQIPtr<IDVBTuneRequest> pIDVBTuneRequest(pITuneRequest);
		if (pIDVBTuneRequest) {
			pIDVBTuneRequest->put_ONID(-1);
			pIDVBTuneRequest->put_TSID(-1);
			pIDVBTuneRequest->put_SID(-1);
		}
	}

	// IChannelTuneRequest特有
	{
		CComQIPtr<IChannelTuneRequest> pIChannelTuneRequest(pITuneRequest);
		if (pIChannelTuneRequest) {
			pIChannelTuneRequest->put_Channel(-1);
		}
	}

	// IATSCChannelTuneRequest特有
	{
		CComQIPtr<IATSCChannelTuneRequest> pIATSCChannelTuneRequest(pITuneRequest);
		if (pIATSCChannelTuneRequest) {
			pIATSCChannelTuneRequest->put_MinorChannel(-1);
		}
	}

	// IDigitalCableTuneRequest特有
	{
		CComQIPtr<IDigitalCableTuneRequest> pIDigitalCableTuneRequest(pITuneRequest);
		if (pIDigitalCableTuneRequest) {
			pIDigitalCableTuneRequest->put_MajorChannel(-1);
			pIDigitalCableTuneRequest->put_SourceID(-1);
		}
	}

	m_pITuner->put_TuningSpace(m_pITuningSpace);

	m_pITuner->put_TuneRequest(pITuneRequest);

	return S_OK;
}

HRESULT CBonTuner::LoadNetworkProvider(void)
{
	static const WCHAR * const FILTER_GRAPH_NAME_NETWORK_PROVIDER[] = {
		L"Microsoft Network Provider",
		L"Microsoft DVB-S Network Provider",
		L"Microsoft DVB-T Network Provider",
		L"Microsoft DVB-C Network Provider",
		L"Microsoft ATSC Network Provider",
	};

	const WCHAR *strName = NULL;
	CLSID clsidNetworkProvider = CLSID_NULL;

	switch (m_nNetworkProvider) {
	case eNetworkProviderGeneric:
		clsidNetworkProvider = CLSID_NetworkProvider;
		strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[0];
		break;
	case eNetworkProviderDVBS:
		clsidNetworkProvider = CLSID_DVBSNetworkProvider;
		strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[1];
		break;
	case eNetworkProviderDVBT:
		clsidNetworkProvider = CLSID_DVBTNetworkProvider;
		strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[2];
		break;
	case eNetworkProviderDVBC:
		clsidNetworkProvider = CLSID_DVBCNetworkProvider;
		strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[3];
		break;
	case eNetworkProviderATSC:
		clsidNetworkProvider = CLSID_ATSCNetworkProvider;
		strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[4];
		break;
	case eNetworkProviderAuto:
	default:
		switch (m_nDVBSystemType) {
		case eTunerTypeDVBS:
		case eTunerTypeISDBS:
			clsidNetworkProvider = CLSID_DVBSNetworkProvider;
			strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[1];
			break;
		case eTunerTypeDVBT:
		case eTunerTypeDVBT2:
		case eTunerTypeISDBT:
			clsidNetworkProvider = CLSID_DVBTNetworkProvider;
			strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[2];
			break;
		case eTunerTypeDVBC:
			clsidNetworkProvider = CLSID_DVBCNetworkProvider;
			strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[3];
			break;
		case eTunerTypeATSC_Antenna:
		case eTunerTypeATSC_Cable:
		case eTunerTypeDigitalCable:
			clsidNetworkProvider = CLSID_ATSCNetworkProvider;
			strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[4];
			break;
		default:
			clsidNetworkProvider = CLSID_NetworkProvider;
			strName = FILTER_GRAPH_NAME_NETWORK_PROVIDER[0];
			break;
		}
		break;
	}

	HRESULT hr;

	if (FAILED(hr = ::CoCreateInstance(clsidNetworkProvider, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)(&m_pNetworkProvider)))) {
		OutputDebug(L"Fail to create network-provider.\n");
		return hr;
	}

	if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pNetworkProvider, strName))) {
		OutputDebug(L"Fail to add network-provider into graph.\n");
		SAFE_RELEASE(m_pNetworkProvider);
		return hr;
	}

	if (FAILED(hr = m_pNetworkProvider->QueryInterface(__uuidof(ITuner), (void **)&m_pITuner))) {
		OutputDebug(L"Fail to get ITuner.\n");
		return E_FAIL;
	}

	return S_OK;
}

void CBonTuner::UnloadNetworkProvider(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pNetworkProvider)
		hr = m_pIGraphBuilder->RemoveFilter(m_pNetworkProvider);

	SAFE_RELEASE(m_pITuner);
	SAFE_RELEASE(m_pNetworkProvider);
}

// ini ファイルで指定されたチューナ・キャプチャの組合せListを作成
HRESULT CBonTuner::InitDSFilterEnum(void)
{
	HRESULT hr;

	// システムに存在するチューナ・キャプチャのリスト
	vector<DSListData> TunerList;
	vector<DSListData> CaptureList;

	ULONG order;

	SAFE_DELETE(m_pDSFilterEnumTuner);
	SAFE_DELETE(m_pDSFilterEnumCapture);

	try {
		m_pDSFilterEnumTuner = new CDSFilterEnum(KSCATEGORY_BDA_NETWORK_TUNER, CDEF_DEVMON_PNP_DEVICE);
	}
	catch (...) {
		OutputDebug(L"[InitDSFilterEnum] Fail to construct CDSFilterEnum(KSCATEGORY_BDA_NETWORK_TUNER).\n");
		return E_FAIL;
	}

	order = 0;
	while (SUCCEEDED(hr = m_pDSFilterEnumTuner->next()) && hr == S_OK) {
		wstring sDisplayName;
		wstring sFriendlyName;

		// チューナの DisplayName, FriendlyName を得る
		m_pDSFilterEnumTuner->getDisplayName(&sDisplayName);
		m_pDSFilterEnumTuner->getFriendlyName(&sFriendlyName);

		// 一覧に追加
		TunerList.emplace_back(sDisplayName, sFriendlyName, order);

		order++;
	}

	try {
		m_pDSFilterEnumCapture = new CDSFilterEnum(KSCATEGORY_BDA_RECEIVER_COMPONENT, CDEF_DEVMON_PNP_DEVICE);
	}
	catch (...) {
		OutputDebug(L"[InitDSFilterEnum] Fail to construct CDSFilterEnum(KSCATEGORY_BDA_RECEIVER_COMPONENT).\n");
		return E_FAIL;
	}

	order = 0;
	while (SUCCEEDED(hr = m_pDSFilterEnumCapture->next()) && hr == S_OK) {
		wstring sDisplayName;
		wstring sFriendlyName;

		// チューナの DisplayName, FriendlyName を得る
		m_pDSFilterEnumCapture->getDisplayName(&sDisplayName);
		m_pDSFilterEnumCapture->getFriendlyName(&sFriendlyName);

		// 一覧に追加

		CaptureList.emplace_back(sDisplayName, sFriendlyName, order);

		order++;
	}

	unsigned int total = 0;
	m_UsableTunerCaptureList.clear();

	for (unsigned int i = 0; i < m_aTunerParam.Tuner.size(); i++) {
		for (vector<DSListData>::iterator it = TunerList.begin(); it != TunerList.end(); it++) {
			// DisplayName に GUID が含まれるか検査して、NOだったら次のチューナへ
			if (m_aTunerParam.Tuner[i]->TunerGUID.compare(L"") != 0 && it->GUID.find(m_aTunerParam.Tuner[i]->TunerGUID) == wstring::npos) {
				continue;
			}

			// FriendlyName が含まれるか検査して、NOだったら次のチューナへ
			if (m_aTunerParam.Tuner[i]->TunerFriendlyName.compare(L"") != 0 && it->FriendlyName.find(m_aTunerParam.Tuner[i]->TunerFriendlyName) == wstring::npos) {
				continue;
			}

			// 対象のチューナデバイスだった
			OutputDebug(L"[InitDSFilterEnum] Found tuner device=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
			if (!m_aTunerParam.bNotExistCaptureDevice) {
				// Captureデバイスを使用する
				vector<DSListData> TempCaptureList;
				for (vector<DSListData>::iterator it2 = CaptureList.begin(); it2 != CaptureList.end(); it2++) {
					// DisplayName に GUID が含まれるか検査して、NOだったら次のキャプチャへ
					if (m_aTunerParam.Tuner[i]->CaptureGUID.compare(L"") != 0 && it2->GUID.find(m_aTunerParam.Tuner[i]->CaptureGUID) == wstring::npos) {
						continue;
					}

					// FriendlyName が含まれるか検査して、NOだったら次のキャプチャへ
					if (m_aTunerParam.Tuner[i]->CaptureFriendlyName.compare(L"") != 0 && it2->FriendlyName.find(m_aTunerParam.Tuner[i]->CaptureFriendlyName) == wstring::npos) {
						continue;
					}

					// 対象のキャプチャデバイスだった
					OutputDebug(L"[InitDSFilterEnum]   Found capture device=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
					TempCaptureList.emplace_back(*it2);
				}

				if (TempCaptureList.empty()) {
					// キャプチャデバイスが見つからなかったので次のチューナへ
					OutputDebug(L"[InitDSFilterEnum]   No combined capture devices.\n");
					continue;
				}

				// チューナをListに追加
				m_UsableTunerCaptureList.emplace_back(*it);

				unsigned int count = 0;
				if (m_aTunerParam.bCheckDeviceInstancePath) {
					// チューナデバイスとキャプチャデバイスのデバイスインスタンスパスが一致しているか確認
					OutputDebug(L"[InitDSFilterEnum]   Checking device instance path.\n");
					wstring::size_type n, last;
					n = last = 0;
					while ((n = it->GUID.find(L'#', n)) != wstring::npos) {
						last = n;
						n++;
					}
					if (last != 0) {
						wstring path = it->GUID.substr(0, last);
						for (vector<DSListData>::iterator it2 = TempCaptureList.begin(); it2 != TempCaptureList.end(); it2++) {
							if (it2->GUID.find(path) != wstring::npos) {
								// デバイスパスが一致するものをListに追加
								OutputDebug(L"[InitDSFilterEnum]     Adding matched tuner and capture device.\n");
								OutputDebug(L"[InitDSFilterEnum]       tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
								OutputDebug(L"[InitDSFilterEnum]       capture=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
								m_UsableTunerCaptureList.back().CaptureList.emplace_back(*it2);
								count++;
							}
						}
					}
				}

				if (count == 0) {
					// デバイスパスが一致するものがなかった or 確認しない
					if (m_aTunerParam.bCheckDeviceInstancePath) {
						OutputDebug(L"[InitDSFilterEnum]     No matched devices.\n");
					}
					for (vector<DSListData>::iterator it2 = TempCaptureList.begin(); it2 != TempCaptureList.end(); it2++) {
						// すべてListに追加
						OutputDebug(L"[InitDSFilterEnum]   Adding tuner and capture device.\n");
						OutputDebug(L"[InitDSFilterEnum]     tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
						OutputDebug(L"[InitDSFilterEnum]     capture=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
						m_UsableTunerCaptureList.back().CaptureList.emplace_back(*it2);
						count++;
					}
				}

				OutputDebug(L"[InitDSFilterEnum]   %d of combination was added.\n", count);
				total += count;
			}
			else
			{
				// Captureデバイスを使用しない
				OutputDebug(L"[InitDSFilterEnum]   Adding tuner device only.\n");
				OutputDebug(L"[InitDSFilterEnum]     tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
				m_UsableTunerCaptureList.emplace_back(*it);
			}
		}
	}
	if (m_UsableTunerCaptureList.empty()) {
		OutputDebug(L"[InitDSFilterEnum] No devices found.\n");
		return E_FAIL;
	}

	OutputDebug(L"[InitDSFilterEnum] Total %d of combination was added.\n", total);
	return S_OK;
}

// チューナ・キャプチャの組合わせリストから動作するものを探す
HRESULT CBonTuner::LoadAndConnectDevice(void)
{
	HRESULT hr;
	if (!m_pITuningSpace || !m_pNetworkProvider) {
		OutputDebug(L"[P->T] TuningSpace or NetworkProvider NOT SET.\n");
		return E_POINTER;
	}

	if (!m_pDSFilterEnumTuner || !m_pDSFilterEnumCapture) {
		OutputDebug(L"[P->T] DSFilterEnum NOT SET.\n");
		return E_POINTER;
	}

	for (list<TunerCaptureList>::iterator it = m_UsableTunerCaptureList.begin(); it != m_UsableTunerCaptureList.end(); it++) {
		OutputDebug(L"[P->T] Trying tuner device=FriendlyName:%s,  GUID:%s\n", it->Tuner.FriendlyName.c_str(), it->Tuner.GUID.c_str());
		// チューナデバイスループ
		// 排他処理用にセマフォ用文字列を作成 ('\' -> '/')
		wstring::size_type n = 0;
		wstring semName = it->Tuner.GUID;
		while ((n = semName.find(L'\\', n)) != wstring::npos) {
			semName.replace(n, 1, 1, L'/');
		}
		semName = L"Global\\" + semName;

		// 排他処理
		m_hSemaphore = ::CreateSemaphoreW(NULL, 1, 1, semName.c_str());
		DWORD result = WaitForSingleObjectWithMessageLoop(m_hSemaphore, 0);
		if (result != WAIT_OBJECT_0) {
			OutputDebug(L"[P->T] Another is using.\n");
			// 使用中なので次のチューナを探す
		} 
		else {
			// 排他確認OK
			// チューナデバイスのフィルタを取得
			if (FAILED(hr = m_pDSFilterEnumTuner->getFilter(&m_pTunerDevice, it->Tuner.Order))) {
				OutputDebug(L"[P->T] Error in Get Filter\n");
			}
			else {
				// フィルタ取得成功
				// チューナデバイスのフィルタを追加
				if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pTunerDevice, it->Tuner.FriendlyName.c_str()))) {
					OutputDebug(L"[P->T] Error in AddFilter\n");
				}
				else {
					// フィルタ取得成功
					// チューナデバイスをconnect してみる
					if (FAILED(hr = Connect(L"Provider->Tuner", m_pNetworkProvider, m_pTunerDevice))) {
						// NetworkProviderが異なる等の理由でconnectに失敗
						// 次のチューナへ
						OutputDebug(L"[P->T] Connect Failed.\n");
					}
					else {
						// connect 成功
						OutputDebug(L"[P->T] Connect OK.\n");

						// チューナ固有Dllが必要なら読込み、固有の初期化処理があれば呼び出す
						if (FAILED(hr = CheckAndInitTunerDependDll(it->Tuner.GUID, it->Tuner.FriendlyName))) {
							// 何らかの理由で使用できないみたいなので次のチューナへ
							OutputDebug(L"[P->T] Discarded by BDASpecials.\n");
						}
						else {
							// 固有Dll処理OK
							if (!m_aTunerParam.bNotExistCaptureDevice) {
								// キャプチャデバイスを使用する
								for (vector<DSListData>::iterator it2 = it->CaptureList.begin(); it2 != it->CaptureList.end(); it2++) {
									OutputDebug(L"[T->C] Trying capture device=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
									// キャプチャデバイスループ
									// チューナ固有Dllでの確認処理があれば呼び出す
									if (FAILED(hr = CheckCapture(it->Tuner.GUID, it->Tuner.FriendlyName, it2->GUID, it2->FriendlyName))) {
										// 固有Dllがダメと言っているので次のキャプチャデバイスへ
										OutputDebug(L"[T->C] Discarded by BDASpecials.\n");
									}
									else {
										// 固有Dllの確認OK
										// キャプチャデバイスのフィルタを取得
										if (FAILED(hr = m_pDSFilterEnumCapture->getFilter(&m_pCaptureDevice, it2->Order))) {
											OutputDebug(L"[T->C] Error in Get Filter\n");
										}
										else {
											// フィルタ取得成功
											// キャプチャデバイスのフィルタを追加
											if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pCaptureDevice, it2->FriendlyName.c_str()))) {
												OutputDebug(L"[T->C] Error in AddFilter\n");
											}
											else {
												// フィルタ追加成功
												// キャプチャデバイスをconnect してみる
												if (FAILED(hr = Connect(L"Tuner->Capture", m_pTunerDevice, m_pCaptureDevice))) {
													// connect できなければチューナとの組合せが正しくないと思われる
													// 次のキャプチャデバイスへ
													OutputDebug(L"[T->C] Connect Failed.\n");
												}
												else {
													// connect 成功
													OutputDebug(L"[T->C] Connect OK.\n");

													// TsWriter以降と接続～Run
													if (FAILED(LoadAndConnectMiscFilters())) {
														// 失敗したら次のキャプチャデバイスへ
													}
													else {
														// すべて成功
														LoadTunerDependCode();
														if (m_bTryAnotherTuner)
															m_UsableTunerCaptureList.splice(m_UsableTunerCaptureList.end(), m_UsableTunerCaptureList, it);
														return S_OK;
													} // すべて成功
													DisconnectAll(m_pCaptureDevice);
												} // connect 成功
												m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);
											} // フィルタ追加成功
											SAFE_RELEASE(m_pCaptureDevice);
										} // フィルタ取得成功
									} // 固有Dllの確認OK
								} // キャプチャデバイスループ
								// 動作する組合せが見つからなかったので次のチューナへ
							} // キャプチャデバイスを使用する
							else {
								// キャプチャデバイスを使用しない
								// TsWriter以降と接続～Run
								if (FAILED(hr = LoadAndConnectMiscFilters())) {
									// 失敗したら次のチューナへ
								}
								else {
									// 成功
									LoadTunerDependCode();
									if (m_bTryAnotherTuner)
										m_UsableTunerCaptureList.splice(m_UsableTunerCaptureList.end(), m_UsableTunerCaptureList, it);
									return S_OK;
								} // 成功
							} // キャプチャデバイスを使用しない
						} // 固有Dll処理OK
						ReleaseTunerDependCode();
						DisconnectAll(m_pTunerDevice);
					} // connect 成功
					m_pIGraphBuilder->RemoveFilter(m_pTunerDevice);
				} // フィルタ取得成功
				SAFE_RELEASE(m_pTunerDevice);
			} // フィルタ取得成功
			::ReleaseSemaphore(m_hSemaphore, 1, NULL);
		} // 排他処理OK
		::CloseHandle(m_hSemaphore);
		m_hSemaphore = NULL;
	} // チューナデバイスループ

	// 動作する組み合わせが見つからなかった
	OutputDebug(L"[P->T] Tuner not found.\n");
	return E_FAIL;
}

void CBonTuner::UnloadTunerDevice(void)
{
	HRESULT hr;

	ReleaseTunerDependCode();

	if (m_pIGraphBuilder && m_pTunerDevice)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTunerDevice);

	SAFE_RELEASE(m_pTunerDevice);
}

void CBonTuner::UnloadCaptureDevice(void)
{
	HRESULT hr;

	if (m_pIGraphBuilder && m_pCaptureDevice)
		hr = m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);

	SAFE_RELEASE(m_pCaptureDevice);
}

HRESULT CBonTuner::LoadAndConnectMiscFilters(void)
{
	HRESULT hr;

	// TsWriterと接続
	if (FAILED(hr = LoadAndConnectTsWriter())) {
		return hr;
	}

	// TsDemuxerと接続
	if (FAILED(hr = LoadAndConnectDemux())) {
		DisconnectAll(m_pTsWriter);
		UnloadTsWriter();
		return hr;
	}

	// TIFと接続
	if (FAILED(hr = LoadAndConnectTif())) {
		DisconnectAll(m_pDemux);
		DisconnectAll(m_pTsWriter);
		UnloadDemux();
		UnloadTsWriter();
		return hr;
	}

	// Runしてみる
	if (FAILED(hr = RunGraph())) {
		OutputDebug(L"RunGraph Failed.\n");
		DisconnectAll(m_pTif);
		DisconnectAll(m_pDemux);
		DisconnectAll(m_pTsWriter);
		UnloadTif();
		UnloadDemux();
		UnloadTsWriter();
		return hr;
	}

	// 成功
	OutputDebug(L"RunGraph OK.\n");
	return S_OK;
}

HRESULT CBonTuner::LoadAndConnectTsWriter(void)
{
	HRESULT hr = E_FAIL;

	if (!m_pTunerDevice || (!m_pCaptureDevice && !m_aTunerParam.bNotExistCaptureDevice)) {
		OutputDebug(L"[C->W] TunerDevice or CaptureDevice NOT SET.\n");
		return E_POINTER;
	}

	// インスタンス作成
	m_pCTsWriter = static_cast<CTsWriter *>(CTsWriter::CreateInstance(NULL, &hr));
	if (!m_pCTsWriter) {
		OutputDebug(L"[C->W] Fail to load TsWriter filter.\n");
		return E_NOINTERFACE;
	}

	m_pCTsWriter->AddRef();

	// フィルタを取得
	if (FAILED(hr = m_pCTsWriter->QueryInterface(IID_IBaseFilter, (void**)(&m_pTsWriter)))) {
		OutputDebug(L"[C->W] Fail to get TsWriter interface.\n");
		SAFE_RELEASE(m_pCTsWriter);
		return hr;
	}

	// フィルタを追加
	if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pTsWriter, FILTER_GRAPH_NAME_TSWRITER))) {
		OutputDebug(L"[C->W] Fail to add TsWriter filter into graph.\n");
		SAFE_RELEASE(m_pTsWriter);
		SAFE_RELEASE(m_pCTsWriter);
		return hr;
	}

	// connect してみる
	if (m_aTunerParam.bNotExistCaptureDevice) {
		// Captureデバイスが存在しない場合はTunerと接続
		if (FAILED(hr = Connect(L"Tuner->TsWriter", m_pTunerDevice, m_pTsWriter))) {
			OutputDebug(L"[T->W] Failed to connect.\n");
			SAFE_RELEASE(m_pTsWriter);
			SAFE_RELEASE(m_pCTsWriter);
			return hr;
		}
	}
	else {
		if (FAILED(hr = Connect(L"Capture->TsWriter", m_pCaptureDevice, m_pTsWriter))) {
			OutputDebug(L"[C->W] Failed to connect.\n");
			SAFE_RELEASE(m_pTsWriter);
			SAFE_RELEASE(m_pCTsWriter);
			return hr;
		}
	}

	// connect 成功なのでこのまま終了
	OutputDebug(L"[C->W] Connect OK.\n");
	return S_OK;
}

void CBonTuner::UnloadTsWriter(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pTsWriter)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTsWriter);

	SAFE_RELEASE(m_pTsWriter);
	SAFE_RELEASE(m_pCTsWriter);
}

HRESULT CBonTuner::LoadAndConnectDemux(void)
{
	HRESULT hr;

	if (!m_pTsWriter) {
			OutputDebug(L"[W->M] TsWriter NOT SET.\n");
			return E_POINTER;
	}

	// インスタンス作成
	if (FAILED(hr = ::CoCreateInstance(CLSID_MPEG2Demultiplexer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)(&m_pDemux)))) {
		OutputDebug(L"[W->M] Fail to load MPEG2-Demultiplexer.\n");
		return hr;
	}

	// フィルタを追加
	if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pDemux, FILTER_GRAPH_NAME_DEMUX))) {
		OutputDebug(L"[W->M] Fail to add MPEG2-Demultiplexer into graph.\n");
		SAFE_RELEASE(m_pDemux);
		return hr;
	}

	// connect してみる
	if (FAILED(hr = Connect(L"Grabber->Demux", m_pTsWriter, m_pDemux))) {
		OutputDebug(L"[W->M] Fail to connect Grabber->Demux.\n");
		SAFE_RELEASE(m_pDemux);
		return hr;
	}

	// connect 成功なのでこのまま終了
	OutputDebug(L"[W->M] Connect OK.\n");
	return S_OK;
}

void CBonTuner::UnloadDemux(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pDemux)
		hr = m_pIGraphBuilder->RemoveFilter(m_pDemux);

	SAFE_RELEASE(m_pDemux);
}

HRESULT CBonTuner::LoadAndConnectTif(void)
{
	HRESULT hr;

	if (!m_pDemux) {
			OutputDebug(L"[M->I] Demux NOT SET.\n");
			return E_POINTER;
	}

	wstring friendlyName;

	try {
		CDSFilterEnum dsfEnum(KSCATEGORY_BDA_TRANSPORT_INFORMATION, CDEF_DEVMON_FILTER);
		while (SUCCEEDED(hr = dsfEnum.next()) && hr == S_OK) {
			// MPEG-2 Sections and Tables Filter に接続してしまうと RunGraph に失敗してしまうので
			// BDA MPEG2 Transport Information Filter 以外はスキップ
			dsfEnum.getFriendlyName(&friendlyName);
			if (friendlyName.find(FILTER_GRAPH_NAME_TIF) == wstring::npos)
				continue;

			// フィルタを取得
			if (FAILED(hr = dsfEnum.getFilter(&m_pTif))) {
				OutputDebug(L"[M->I] Error in Get Filter\n");
				return hr;
			}

			// フィルタを追加
			if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pTif, FILTER_GRAPH_NAME_TIF))) {
				SAFE_RELEASE(m_pTif);
				OutputDebug(L"[M->I] Error in AddFilter.\n");
				return hr;
			}

			// connect してみる
			if (FAILED(hr = Connect(L"Demux -> Tif", m_pDemux, m_pTif))) {
				m_pIGraphBuilder->RemoveFilter(m_pTif);
				SAFE_RELEASE(m_pTif);
				return hr;
			}

			// connect 成功なのでこのまま終了
			OutputDebug(L"[M->I] Connect OK.\n");
			return S_OK;
		}
		OutputDebug(L"[M->I] MPEG2 Transport Information Filter not found.\n");
		return E_FAIL;
	} catch (...) {
		OutputDebug(L"[M->I] Fail to construct CDSFilterEnum.\n");
		SAFE_RELEASE(m_pTif);
		return E_FAIL;
	}
}

void CBonTuner::UnloadTif(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pTif)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTif);

	SAFE_RELEASE(m_pTif);
}

HRESULT CBonTuner::LoadTunerSignalStatistics(void)
{
	HRESULT hr;

	if (m_pTunerDevice == NULL) {
		OutputDebug(L"[LoadTunerSignalStatistics] TunerDevice NOT SET.\n");
		return E_POINTER;
	}

	CComQIPtr<IBDA_Topology> pIBDA_Topology(m_pTunerDevice);
	if (!pIBDA_Topology) {
		OutputDebug(L"[LoadTunerSignalStatistics] Fail to get IBDA_Topology.\n");
		return E_FAIL;
	}

	ULONG NodeTypes;
	ULONG NodeType[32];
	if (FAILED(hr = pIBDA_Topology->GetNodeTypes(&NodeTypes, 32, NodeType))) {
		OutputDebug(L"[LoadTunerSignalStatistics] Fail to get NodeTypes.\n");
		return E_FAIL;
	}

	BOOL bFound = FALSE;
	for (ULONG i = 0; i < NodeTypes; i++) {
		IUnknown *pControlNode = NULL;
		if (SUCCEEDED(hr = pIBDA_Topology->GetControlNode(0UL, 1UL, NodeType[i], &pControlNode))) {
			if (SUCCEEDED(hr = pControlNode->QueryInterface(__uuidof(IBDA_SignalStatistics), (void **)(&m_pIBDA_SignalStatistics)))) {
				OutputDebug(L"[LoadTunerSignalStatistics] SUCCESS.\n");
				bFound = TRUE;
			}
			SAFE_RELEASE(pControlNode);
		}
		if (bFound)
			break;
	}

	if (!m_pIBDA_SignalStatistics) {
		OutputDebug(L"[LoadTunerSignalStatistics] Fail to get IBDA_SignalStatistics.\n");
		return E_FAIL;
	}

	return S_OK;
}

void CBonTuner::UnloadTunerSignalStatistics(void)
{
	SAFE_RELEASE(m_pIBDA_SignalStatistics);
}


// Connect pins (Common subroutine)
//  全てのピンを接続して成功したら終了
//
HRESULT CBonTuner::Connect(const WCHAR* pszName, IBaseFilter* pFilterUp, IBaseFilter* pFilterDown)
{
	HRESULT hr;

	IEnumPins *pIEnumPinsUp = NULL;
	IEnumPins *pIEnumPinsDown = NULL;
	do {
		// 上流フィルタのピン列挙
		if (FAILED(hr = pFilterUp->EnumPins(&pIEnumPinsUp))) {
			OutputDebug(L"Fatal Error; Cannot enumerate upstream filter's pins.\n");
			break;
		}

		// 下流フィルタのピン列挙
		if (FAILED(hr = pFilterDown->EnumPins(&pIEnumPinsDown))) {
			OutputDebug(L"Fatal Error; Cannot enumerate downstream filter's pins.\n");
			break;
		}

		BOOL bExitLoop = FALSE;
		// 上流フィルタのピンの数だけループ
		IPin *pIPinUp = NULL;
		while (SUCCEEDED(hr = pIEnumPinsUp->Next(1, &pIPinUp, 0)) && hr == S_OK) {
			PIN_INFO PinInfoUp = { NULL, };
			IPin *pIPinPeerOfUp = NULL;
			do {
				if (FAILED(hr = pIPinUp->QueryPinInfo(&PinInfoUp))) {
					OutputDebug(L"Fatal Error; Cannot get upstream filter's pinInfo.\n");
					bExitLoop = TRUE;
					break;
				}

				// 着目ピンが INPUTピンなら次の上流ピンへ
				if (PinInfoUp.dir == PINDIR_INPUT) {
					OutputDebug(L"This is an INPUT pin.\n");
					break;
				}

				// 上流フィルタの着目ピンが接続済orエラーだったら次の上流ピンへ
				if (pIPinUp->ConnectedTo(&pIPinPeerOfUp) != VFW_E_NOT_CONNECTED){
					OutputDebug(L"Target pin is already connected.\n");
					break;
				}

				// 下流フィルタのピンの数だけループ
				IPin *pIPinDown = NULL;
				pIEnumPinsDown->Reset();
				while (SUCCEEDED(hr = pIEnumPinsDown->Next(1, &pIPinDown, 0)) && hr == S_OK) {
					PIN_INFO PinInfoDown = { NULL, };
					IPin *pIPinPeerOfDown = NULL;
					do {
						if (FAILED(hr = pIPinDown->QueryPinInfo(&PinInfoDown))) {
							OutputDebug(L"Fatal Error; cannot get downstream filter's pinInfo.\n");
							bExitLoop = TRUE;
							break;
						}

						// 着目ピンが OUTPUT ピンなら次の下流ピンへ
						if (PinInfoDown.dir == PINDIR_OUTPUT) {
							OutputDebug(L"This is an OUTPUT pin.\n");
							break;
						}

						// 下流フィルタの着目ピンが接続済orエラーだったら次の下流ピンへ
						if (pIPinDown->ConnectedTo(&pIPinPeerOfDown) != VFW_E_NOT_CONNECTED) {
							OutputDebug(L"Target pin is already connected.\n");
							break;
						}

						// 接続を試みる
						if (SUCCEEDED(hr = m_pIGraphBuilder->ConnectDirect(pIPinUp, pIPinDown, NULL))) {
							OutputDebug(L"%s CBonTuner::Connect successfully.\n", pszName);
							bExitLoop = TRUE;
							break;
						} else {
							// 違うチューナユニットのフィルタを接続しようとしてる場合など
							// コネクトできない場合、次の下流ピンへ
							OutputDebug(L"Can't connect to unconnected pin, Maybe differenct unit?\n");
						}
					} while(0);
					SAFE_RELEASE(pIPinPeerOfDown);
					SAFE_RELEASE(PinInfoDown.pFilter);
					SAFE_RELEASE(pIPinDown);
					if (bExitLoop)
						break;
					OutputDebug(L"Trying next downstream pin.\n");
				} // while; 次の下流ピンへ
				break;
			} while (0);
			SAFE_RELEASE(pIPinPeerOfUp);
			SAFE_RELEASE(PinInfoUp.pFilter);
			SAFE_RELEASE(pIPinUp);
			if (bExitLoop)
				break;
			OutputDebug(L"Trying next upstream pin.\n");
		} // while ; 次の上流ピンへ
		if (!bExitLoop) {
			OutputDebug(L"Can't connect.\n");
			hr = E_FAIL;
		}
	} while(0);
	SAFE_RELEASE(pIEnumPinsDown);
	SAFE_RELEASE(pIEnumPinsUp);

	return hr;
}

void CBonTuner::DisconnectAll(IBaseFilter* pFilter)
{
	if (!m_pIGraphBuilder || !pFilter)
		return;
	
	HRESULT hr;

	IEnumPins *pIEnumPins = NULL;
	// フィルタのピン列挙
	if (SUCCEEDED(hr = pFilter->EnumPins(&pIEnumPins))) {
		// ピンの数だけループ
		IPin *pIPin = NULL;
		while (SUCCEEDED(hr = pIEnumPins->Next(1, &pIPin, 0)) && hr == S_OK) {
			// ピンが接続済だったら切断
			IPin *pIPinPeerOf = NULL;
			if (SUCCEEDED(hr = pIPin->ConnectedTo(&pIPinPeerOf))) {
				hr = m_pIGraphBuilder->Disconnect(pIPinPeerOf);
				hr = m_pIGraphBuilder->Disconnect(pIPin);
				SAFE_RELEASE(pIPinPeerOf);
			}
			SAFE_RELEASE(pIPin);
		}
		SAFE_RELEASE(pIEnumPins);
	}
}

CBonTuner::TS_BUFF::TS_BUFF(void)
	: TempBuff(NULL),
	TempOffset(0),
	BuffSize(0),
	MaxCount(0)
{
	::InitializeCriticalSection(&cs);
}

CBonTuner::TS_BUFF::~TS_BUFF(void)
{
	Purge();
	SAFE_DELETE_ARRAY(TempBuff);
	::DeleteCriticalSection(&cs);
}

void CBonTuner::TS_BUFF::SetSize(DWORD dwBuffSize, DWORD dwMaxCount)
{
	Purge();
	SAFE_DELETE_ARRAY(TempBuff);
	if (dwBuffSize) {
		TempBuff = new BYTE[dwBuffSize];
	}
	BuffSize = dwBuffSize;
	MaxCount = dwMaxCount;
}

void CBonTuner::TS_BUFF::Purge(void)
{
	// 受信TSバッファ
	::EnterCriticalSection(&cs);
	while (!List.empty()) {
		SAFE_DELETE(List.front());
		List.pop();
	}
	TempOffset = 0;
	::LeaveCriticalSection(&cs);
}

void CBonTuner::TS_BUFF::Add(TS_DATA *pItem)
{
	::EnterCriticalSection(&cs);
	while (List.size() >= MaxCount) {
		// オーバーフローなら古いものを消す
		SAFE_DELETE(List.front());
		List.pop();
	}
	List.push(pItem);
	::LeaveCriticalSection(&cs);
}

BOOL CBonTuner::TS_BUFF::AddData(BYTE *pbyData, DWORD dwSize)
{
	BOOL ret = false;
	while (dwSize) {
		TS_DATA *pItem = NULL;
		::EnterCriticalSection(&cs);
		if (TempBuff) {
			// iniファイルでBuffSizeが指定されている場合はそのサイズに合わせる
			DWORD dwCopySize = (BuffSize > TempOffset + dwSize) ? dwSize : BuffSize - TempOffset;
			::CopyMemory(TempBuff + TempOffset, pbyData, dwCopySize);
			TempOffset += dwCopySize;
			dwSize -= dwCopySize;
			pbyData += dwCopySize;

			if (TempOffset >= BuffSize) {
				// テンポラリバッファのデータを追加
				pItem = new TS_DATA(TempBuff, TempOffset, FALSE);
				TempBuff = new BYTE[BuffSize];
				TempOffset = 0;
			}
		}
		else {
			// BuffSizeが指定されていない場合は上流から受け取ったサイズでそのまま追加
			pItem = new TS_DATA(pbyData, dwSize, TRUE);
			dwSize = 0;
		}

		if (pItem) {
			// FIFOへ追加
			while (List.size() >= MaxCount) {
				// オーバーフローなら古いものを消す
				SAFE_DELETE(List.front());
				List.pop();
			}
			List.push(pItem);
			ret = TRUE;
		}
		::LeaveCriticalSection(&cs);
	}
	return ret;
}

CBonTuner::TS_DATA * CBonTuner::TS_BUFF::Get(void)
{
	TS_DATA *ts = NULL;
	::EnterCriticalSection(&cs);
	if (!List.empty()) {
		ts = List.front();
		List.pop();
	}
	::LeaveCriticalSection(&cs);
	return ts;
}

size_t CBonTuner::TS_BUFF::Size(void)
{
	return List.size();
}
