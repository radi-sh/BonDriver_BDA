// BonTuner.cpp: CBonTuner クラスのインプリメンテーション
//
//////////////////////////////////////////////////////////////////////

#include <Windows.h>
#include <stdio.h>

#include "BonTuner.h"

#include "common.h"

#include "DSFilterEnum.h"
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

// transform()
#include <algorithm>

#pragma comment(lib, "Strmiids.lib")
#pragma comment(lib, "ksproxy.lib")

#ifdef _DEBUG
#pragma comment(lib, "strmbasd.lib")
#else
#pragma comment(lib, "strmbase.lib")
#endif

#pragma comment(lib, "winmm.lib")

//////////////////////////////////////////////////////////////////////
// 定数等定義
//////////////////////////////////////////////////////////////////////

// TS Writerの名称:AddFilter時に名前を登録するだけなので何でもよい
static const WCHAR *FILTER_GRAPH_NAME_TSWRITER	= L"TS Writer";

// MPEG2 Demultiplexerの名称:AddFilter時に名前を登録するだけなので何でもよい
static const WCHAR *FILTER_GRAPH_NAME_DEMUX = L"MPEG2 Demultiplexer";

// MPEG2 TIFの名称:CLSIDだけでは特定できないのでこの名前と一致するものを使用する
static const WCHAR *FILTER_GRAPH_NAME_TIF = L"BDA MPEG2 Transport Information Filter";

// Network Providerの名称:AddFilter時に名前を登録するだけなので何でもよい
static const WCHAR *FILTER_GRAPH_NAME_NETWORK_PROVIDER = L"Network Provider";

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
	m_nSignalLevelCalcType(0),
	m_fStrengthCoefficient(1),
	m_fQualityCoefficient(1),
	m_dwBuffSize(188 * 1024),
	m_dwMaxBuffCount(512),
	m_nWaitTsCount(1),
	m_nWaitTsSleep(100),
	m_bReserveUnusedCh(FALSE),
	m_szIniFilePath(L""),
	m_hOnStreamEvent(NULL),
	m_hOnDecodeEvent(NULL),
	m_LastBuff(NULL),
	m_pbyRecvBuff(NULL),
	m_dwBuffOffset(0),
	m_bRecvStarted(FALSE),
	m_hSemaphore(NULL),
	m_pITuningSpace(NULL),
	m_pNetworkProvider(NULL),
	m_pTunerDevice(NULL),
	m_pCaptureDevice(NULL),
	m_pTsWriter(NULL),
	m_pDemux(NULL),
	m_pTif(NULL),
	m_pIGraphBuilder(NULL),
	m_pIMediaControl(NULL), 
	m_pCTsWriter(NULL),
	m_nDVBSystemType(eTunerTypeDVBS),
	m_nDefaultNetwork(1),
	m_bOpened(FALSE),
	m_dwCurSpace(CBonTuner::SPACE_INVALID),
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

	m_pbyRecvBuff = new BYTE[m_dwBuffSize];

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

	SAFE_DELETE_ARRAY(m_pbyRecvBuff);

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

		// チューナ・キャプチャ以後の構築と実行
		if (FAILED(hr = LoadAndConnectTunerDevice()))
			break;

		OutputDebug(L"Build graph Successfully.\n");

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
		::WaitForSingleObject(m_aDecodeProc.hThread, INFINITE);
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
	if (m_LastBuff != NULL) {
		SAFE_DELETE(m_LastBuff);
	}

	// グラフ解放
	CleanupGraph();

	m_dwCurSpace = CBonTuner::SPACE_INVALID;
	m_dwCurChannel = CBonTuner::CHANNEL_INVALID;
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
	float ret = 0;

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
	// IBdaSpecials2固有関数があれば丸投げ
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->GetSignalStrength(&f)) != E_NOINTERFACE) {
		return f;
	}

	//   get_SignalQuality 信号の品質を示す 1 ～ 100 の値を取得する。
	//   get_SignalStrength デシベル単位の信号の強度を示す値を取得する。 
	int nStrength;
	int nQuality;
	int nLock;

	if (m_dwCurChannel == CBonTuner::CHANNEL_INVALID)
		// チャンネル番号不明時は0を返す
		return 0;

	GetSignalState(&nStrength, &nQuality, &nLock);
	if (!nLock)
		// Lock出来ていない場合は0を返す
		return 0;
	if (nStrength < 0 && (m_nSignalLevelCalcType == 0 || m_nSignalLevelCalcType == 2))
		// Strengthは-1を返す場合がある
		return (float)nStrength;
	float s = 1.0F;
	float q = 1.0F;
	if (m_nSignalLevelCalcType == 0 || m_nSignalLevelCalcType == 2)
		s = float(nStrength) / m_fStrengthCoefficient;
	if (m_nSignalLevelCalcType == 1 || m_nSignalLevelCalcType == 2)
		q = float(nQuality) / m_fQualityCoefficient;
	return s * q;
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
	return (DWORD)m_DecodedTsBuff.size();
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
	if (m_LastBuff != NULL) {
		SAFE_DELETE(m_LastBuff);
	}
	BOOL bRet = TRUE;
	// 判定の前後で条件が変ってはまずいのでここで CriticalSection しておく
	::EnterCriticalSection(&m_csDecodedTSBuff);
	if (m_DecodedTsBuff.size() != 0) {
		m_LastBuff = m_DecodedTsBuff[0];
		m_DecodedTsBuff.erase(m_DecodedTsBuff.begin());
		::LeaveCriticalSection(&m_csDecodedTSBuff);
		*pdwSize = m_LastBuff->dwSize;
		*ppDst = m_LastBuff->pbyBuff;
		*pdwRemain = (DWORD)m_DecodedTsBuff.size();
	} else {
		::LeaveCriticalSection(&m_csDecodedTSBuff);
		*pdwSize = 0;
		*pdwRemain = 0;
		bRet = FALSE;
	}
	return bRet;
}

void CBonTuner::PurgeTsStream(void)
{
	// m_LastBuff は参照されている可能性があるので delete しない

	// 受信TSバッファ
	::EnterCriticalSection(&m_csTSBuff);
	for (int i = 0; i < (int)m_TsBuff.size(); i++) {
		SAFE_DELETE(m_TsBuff[i])
	}
	m_TsBuff.clear();
	m_dwBuffOffset = 0;
	::LeaveCriticalSection(&m_csTSBuff);

	// デコード後TSバッファ
	::EnterCriticalSection(&m_csDecodedTSBuff);
	for (int i = 0; i < (int)m_DecodedTsBuff.size(); i++) {
		SAFE_DELETE(m_DecodedTsBuff[i])
	}
	m_DecodedTsBuff.clear();
	::LeaveCriticalSection(&m_csDecodedTSBuff);
}

LPCTSTR CBonTuner::GetTunerName(void)
{
	return m_aTunerParam.sTunerName.c_str();
}

const BOOL CBonTuner::IsTunerOpening(void)
{
	return m_bOpened;
}

LPCTSTR CBonTuner::EnumTuningSpace(const DWORD dwSpace)
{
	if (dwSpace < m_TuningData.dwNumSpace) {
		map<int, TuningSpaceData*>::iterator it = m_TuningData.Spaces.find(dwSpace);
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
	map<int, TuningSpaceData*>::iterator it = m_TuningData.Spaces.find(dwSpace);
	if (it != m_TuningData.Spaces.end()) {
		if (dwChannel < it->second->dwNumChannel) {
			map<int, ChData*>::iterator it2 = it->second->Channels.find(dwChannel);
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

	m_dwCurSpace = CBonTuner::SPACE_INVALID;
	m_dwCurChannel = CBonTuner::CHANNEL_INVALID;

	map<int, TuningSpaceData*>::iterator it = m_TuningData.Spaces.find(dwSpace);
	if (it == m_TuningData.Spaces.end()) {
		OutputDebug(L"    Invalid channel space.\n");
		return FALSE;
	}

	if (dwChannel >= it->second->dwNumChannel) {
		OutputDebug(L"    Invalid channel number.\n");
		return FALSE;
	}

	map<int, ChData*>::iterator it2 = it->second->Channels.find(dwChannel);
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
	TuningParam param;
	param.Frequency = Ch->Frequency;
	param.Polarisation = PolarisationMapping[Ch->Polarisation];
	param.Antenna = &m_aSatellite[Ch->Satellite].Polarisation[Ch->Polarisation];
	param.Modulation = &m_aModulationType[Ch->ModulationType];
	param.ONID = Ch->ONID;
	param.TSID = Ch->TSID;
	param.SID = Ch->SID;

	BOOL bRet = LockChannel(&param);

	// IBdaSpecialsで追加の処理が必要なら行う
	if (m_pIBdaSpecials2)
		hr = m_pIBdaSpecials2->PostLockChannel(&param);

	::Sleep(100);
	PurgeTsStream();
	m_bRecvStarted = TRUE;

	if (bRet) {
		OutputDebug(L"SetChannel success.\n");
		m_dwCurChannel = dwChannel;
		return TRUE;
	}
	// m_byCurTone = CBonTuner::TONE_UNKNOWN;

	OutputDebug(L"SetChannel failed.\n");
	return FALSE;
}

const DWORD CBonTuner::GetCurSpace(void)
{
	return m_dwCurSpace;
}

const DWORD CBonTuner::GetCurChannel(void)
{
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
		DWORD ret = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
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
				pSys->_CloseTuner();
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqSetChannel:
				pCOMProc->uRetVal.SetChannel = pSys->_SetChannel(pCOMProc->uParam.SetChannel.dwSpace, pCOMProc->uParam.SetChannel.dwChannel);
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqGetSignalLevel:
				pCOMProc->uRetVal.GetSignalLevel = pSys->_GetSignalLevel();
				::SetEvent(pCOMProc->hEndEvent);
				break;
			default:
				break;
			}
			break;
		case WAIT_FAILED:
		default:
			DWORD err = ::GetLastError();
			OutputDebug(L"COMProcThread: Unknown error (ret=%d, LastError=0x%08x).\n", ret, err);
			terminate = TRUE;
			break;
		}
	}

	::CoUninitialize();
	OutputDebug(L"COMProcThread: Thread terminated.\n");

	return 0;
}

DWORD WINAPI CBonTuner::DecodeProcThread(LPVOID lpParameter)
{
	BOOL terminate = FALSE;
	CBonTuner* pSys = (CBonTuner*)lpParameter;
	DecodeProc* pDecodeProc = &pSys->m_aDecodeProc;

	DWORD dwMaxBuffCount = pSys->m_dwMaxBuffCount;
	vector<TS_DATA*> *pTsBuff = &pSys->m_TsBuff;
	vector<TS_DATA*> *pDecodedTsBuff = &pSys->m_DecodedTsBuff;
	unsigned int nWaitTsCount = pSys->m_nWaitTsCount;
	CRITICAL_SECTION *pcsTSBuff = &pSys->m_csTSBuff;
	CRITICAL_SECTION *pcsDecodedTSBuff = &pSys->m_csDecodedTSBuff;
	HANDLE *phOnStreamEvent = &pSys->m_hOnStreamEvent;
	HANDLE *phOnDecodeEvent = &pSys->m_hOnDecodeEvent;
	IBdaSpecials2a *pIBdaSpecials2 = *(&pSys->m_pIBdaSpecials2);
	BOOL bNeedDecode = FALSE;

	HRESULT hr;

	OutputDebug(L"DecodeProcThread: Thread created.\n");

	// COM初期化
	hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);

	// IBdaSpecialsによるデコード処理が必要かどうか
	BOOL b = FALSE;
	if (pIBdaSpecials2 && SUCCEEDED(hr = pIBdaSpecials2->IsDecodingNeeded(&b))) {
		if (b)
			bNeedDecode = TRUE;
	}
	OutputDebug(L"DecodeProcThread: Detected IBdaSpecials decoding=%d.\n", bNeedDecode);

	HANDLE h[2] = {
		pDecodeProc->hTerminateRequest,
		*phOnStreamEvent
	};

	while (!terminate) {
		DWORD ret = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
		switch (ret)
		{
		case WAIT_OBJECT_0:
			terminate = TRUE;
			break;
		case WAIT_OBJECT_0 + 1:
			{
				// TSバッファからのデータ取得
				TS_DATA *pBuff = NULL;
				::EnterCriticalSection(pcsTSBuff);
				if (pTsBuff->size() != 0) {
					pBuff = (*pTsBuff)[0];
					pTsBuff->erase(pTsBuff->begin());
				}
				::LeaveCriticalSection(pcsTSBuff);

				if (pBuff) {
					// 必要ならばIBdaSpecialsによるデコード処理を行う
					if (bNeedDecode) {
						pIBdaSpecials2->Decode(pBuff->pbyBuff, pBuff->dwSize);
					}

					::EnterCriticalSection(pcsDecodedTSBuff);

					// 取得したバッファをデコード済みバッファに追加
					while (pDecodedTsBuff->size() >= dwMaxBuffCount) {
						// オーバーフローなら古いものを消す
						SAFE_DELETE((*pDecodedTsBuff)[0]);
						pDecodedTsBuff->erase(pDecodedTsBuff->begin());
					}
					pDecodedTsBuff->push_back(pBuff);

					// 受信イベントセット
					if (pDecodedTsBuff->size() >= nWaitTsCount)
						::SetEvent(*phOnDecodeEvent);

					::LeaveCriticalSection(pcsDecodedTSBuff);
				}
			}
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
	BOOL *pbRecvStarted = &pSys->m_bRecvStarted;
	DWORD dwBuffSize = pSys->m_dwBuffSize;
	BYTE *pbyRecvBuff = pSys->m_pbyRecvBuff;
	DWORD *pdwBuffOffset = &pSys->m_dwBuffOffset;
	DWORD dwMaxBuffCount = pSys->m_dwMaxBuffCount;
	vector<TS_DATA*> *pTsBuff = &pSys->m_TsBuff;
	CRITICAL_SECTION *pcsTSBuff = &pSys->m_csTSBuff;
	HANDLE *phOnStreamEvent = &pSys->m_hOnStreamEvent;

	while (dwSize > 0 && *pbRecvStarted) {
		// 途中でPurgeTsStream されると困るのでここで EnterCriticalSectionしておく
		::EnterCriticalSection(pcsTSBuff);
		DWORD dwCopySize = (dwBuffSize > *pdwBuffOffset + dwSize) ? dwSize : dwBuffSize - *pdwBuffOffset;
		::CopyMemory(pbyRecvBuff + *pdwBuffOffset, pbData, dwCopySize);
		*pdwBuffOffset += dwCopySize;
		dwSize -= dwCopySize;
		pbData += dwCopySize;

		// テンポラリバッファが完成している
		if (*pdwBuffOffset >= dwBuffSize) {
			// テンポラリバッファから新規TSバッファへコピー
			TS_DATA* pItem = new TS_DATA;
			pItem->dwSize = dwBuffSize;
			pItem->pbyBuff = new BYTE[dwBuffSize];
			::CopyMemory(pItem->pbyBuff, pbyRecvBuff, dwBuffSize);
			*pdwBuffOffset = 0;

			// FIFOへ追加
			while(pTsBuff->size() >= dwMaxBuffCount) {
				// オーバーフローなら古いものを消す
				SAFE_DELETE((*pTsBuff)[0]);
				pTsBuff->erase(pTsBuff->begin());
			}
			pTsBuff->push_back(pItem);
			// 受信イベントセット
			::SetEvent(*phOnStreamEvent);
		}
		::LeaveCriticalSection(pcsTSBuff);
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

	for (int i = 0; i < MAX_GUID; i++) {
		WCHAR keyname[64];
		// GUID0 - GUID9: TunerデバイスのGUID ... 指定されなければ見つかった順に使う事を意味する。
		::swprintf_s(keyname, 64, L"GUID%01d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf, 256, m_szIniFilePath);
		strBuf = buf;
		::transform(strBuf.begin(), strBuf.end(), strBuf.begin(), towlower);
		m_aTunerParam.sTunerGUID[i] = strBuf.c_str();

		// FriendlyName0 - FriendlyName9: TunerデバイスのFriendlyName ... 指定されなければ見つかった順に使う事を意味する。
		::swprintf_s(keyname, 64, L"FriendlyName%01d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf, 256, m_szIniFilePath);
		m_aTunerParam.sTunerFriendlyName[i] = buf;

		// CaptureGUID0 - CaptureGUID9: CaptureデバイスのGUID ... 指定されなければ接続可能なデバイスを検索する。
		::swprintf_s(keyname, 64, L"CaptureGUID%01d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf, 256, m_szIniFilePath);
		strBuf = buf;
		::transform(strBuf.begin(), strBuf.end(), strBuf.begin(), towlower);
		m_aTunerParam.sCaptureGUID[i] = strBuf.c_str();

		// CaptureFriendlyName0 - CaptureFriendlyName9: CaptureデバイスのFriendlyName ... 指定されなければ接続可能なデバイスを検索する。
		::swprintf_s(keyname, 64, L"CaptureFriendlyName%01d", i);
		::GetPrivateProfileStringW(L"TUNER", keyname, L"", buf, 256, m_szIniFilePath);
		m_aTunerParam.sCaptureFriendlyName[i] = buf;
	}

	// GUIDが指定されていればGUID0に上書き
	::GetPrivateProfileStringW(L"TUNER", L"GUID", m_aTunerParam.sTunerGUID[0].c_str(), buf, 256, m_szIniFilePath);
	strBuf = buf;
	::transform(strBuf.begin(), strBuf.end(), strBuf.begin(), towlower);
	m_aTunerParam.sTunerGUID[0] = strBuf.c_str();

	// FriendlyNameが指定されていればFriendlyName0に上書き
	::GetPrivateProfileStringW(L"TUNER", L"FriendlyName", m_aTunerParam.sTunerFriendlyName[0].c_str(), buf, 256, m_szIniFilePath);
	m_aTunerParam.sTunerFriendlyName[0] = buf;

	// CaptureGUIDが指定されていればCaptureGUID0に上書き
	::GetPrivateProfileStringW(L"TUNER", L"CaptureGUID", m_aTunerParam.sCaptureGUID[0].c_str(), buf, 256, m_szIniFilePath);
	strBuf = buf;
	::transform(strBuf.begin(), strBuf.end(), strBuf.begin(), towlower);
	m_aTunerParam.sCaptureGUID[0] = strBuf.c_str();

	// CaptureFriendlyNameが指定されていればCaptureFriendlyName0に上書き
	::GetPrivateProfileStringW(L"TUNER", L"CaptureFriendlyName", m_aTunerParam.sCaptureFriendlyName[0].c_str(), buf, 256, m_szIniFilePath);
	m_aTunerParam.sCaptureFriendlyName[0] = buf;

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
	m_nLockWait = (DWORD)::GetPrivateProfileIntW(L"TUNER", L"ChannelLockWait", 2000, m_szIniFilePath);

	// CH切替後のLock確認Delay時間
	m_nLockWaitDelay = (DWORD)::GetPrivateProfileIntW(L"TUNER", L"ChannelLockWaitDelay", 0, m_szIniFilePath);

	// CH切替後のLock確認Retry回数
	m_nLockWaitRetry = (DWORD)::GetPrivateProfileIntW(L"TUNER", L"ChannelLockWaitRetry", 0, m_szIniFilePath);

	// CH切替動作を強制的に2度行うかどうか
	m_bLockTwice = !!(::GetPrivateProfileIntW(L"TUNER", L"ChannelLockTwice", 0, m_szIniFilePath));

	// CH切替動作を強制的に2度行う場合のDelay時間
	m_nLockTwiceDelay = (DWORD)::GetPrivateProfileIntW(L"TUNER", L"ChannelLockTwiceDelay", 100, m_szIniFilePath);

	// Tuning Space名（互換用）
	::GetPrivateProfileStringW(L"TUNER", L"TuningSpaceName", L"スカパー", buf, 64, m_szIniFilePath);
	wstring sTempTuningSpaceName = buf;

	// SignalLevel 算出方法
	// 0 .. Strength値 / StrengthCoefficient
	// 1 .. Quality値 / QualityCoefficient
	// 2 .. (Strength値 / StrengthCoefficient) * (Quality値 / QualityCoefficient)
	m_nSignalLevelCalcType = ::GetPrivateProfileIntW(L"TUNER", L"SignalLevelCalcType", 0, m_szIniFilePath);

	// Strength 値補正係数
	::GetPrivateProfileStringW(L"TUNER", L"StrengthCoefficient", L"1.0", buf, 256, m_szIniFilePath);
	m_fStrengthCoefficient = (float)::_wtof(buf);
	if (m_fStrengthCoefficient == 0)
		m_fStrengthCoefficient = 1;

	// Quality 値補正係数
	::GetPrivateProfileStringW(L"TUNER", L"QualityCoefficient", L"1.0", buf, 256, m_szIniFilePath);
	m_fQualityCoefficient = (float)::_wtof(buf);
	if (m_fQualityCoefficient == 0)
		m_fQualityCoefficient = 1;

	// チューナーの使用するTuningSpace / NetworkProvider等の種類
	//    1 ..DVB - S
	//    2 ..DVB - T
	m_nDVBSystemType = ::GetPrivateProfileIntW(L"TUNER", L"DVBSystemType", 1, m_szIniFilePath);

	// 衛星受信パラメータ/変調方式パラメータのデフォルト値
	//    1 ..SPHD
	//    2 ..BS/CS110
	//    3 ..UHF/CATV
	m_nDefaultNetwork = ::GetPrivateProfileIntW(L"TUNER", L"DefaultNetwork", 1, m_szIniFilePath);

	//
	// BonDriver セクション
	//

	// ストリームデータバッファ1個分のサイズ
	// 188×設定数(bytes)
	m_dwBuffSize = 188 * (DWORD)::GetPrivateProfileIntW(L"BONDRIVER", L"BuffSize", 1024, m_szIniFilePath);

	// ストリームデータバッファの最大個数
	m_dwMaxBuffCount = (DWORD)::GetPrivateProfileIntW(L"BONDRIVER", L"MaxBuffCount", 512, m_szIniFilePath);

	// WaitTsStream時、指定された個数分のストリームデータバッファが貯まるまで待機する
	// チューナのCPU負荷が高いときは数値を大き目にすると効果がある場合もある
	m_nWaitTsCount = ::GetPrivateProfileIntW(L"BONDRIVER", L"WaitTsCount", 1, m_szIniFilePath);
	if (m_nWaitTsCount < 1)
		m_nWaitTsCount = 1;

	// WaitTsStream時ストリームデータバッファが貯まっていない場合に最低限待機する時間(msec)
	// チューナのCPU負荷が高いときは100msec程度を指定すると効果がある場合もある
	m_nWaitTsSleep = ::GetPrivateProfileIntW(L"BONDRIVER", L"WaitTsSleep", 100, m_szIniFilePath);

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
		m_aSatellite[1].Polarisation[1].Oscillator = 11200000;	// 垂直偏波時LNB周波数
		m_aSatellite[1].Polarisation[1].Tone = 0;				// 垂直偏波時トーン信号
		m_aSatellite[1].Polarisation[2].Oscillator = 11200000;	// 水平偏波時LNB周波数
		m_aSatellite[1].Polarisation[2].Tone = 0;				// 水平偏波時トーン信号

		// 衛星設定2（JCSAT-4B）
		m_sSatelliteName[2] = L"124.0E";						// チャンネル名生成用衛星名称
		m_aSatellite[2].Polarisation[1].Oscillator = 11200000;	// 垂直偏波時LNB周波数
		m_aSatellite[2].Polarisation[1].Tone = 1;				// 垂直偏波時トーン信号
		m_aSatellite[2].Polarisation[2].Oscillator = 11200000;	// 水平偏波時LNB周波数
		m_aSatellite[2].Polarisation[2].Tone = 1;				// 水平偏波時トーン信号
		break;

	case 2:
		// BS/CS110
		// 衛星設定1
		m_sSatelliteName[1] = L"BS/CS110";						// チャンネル名生成用衛星名称
		m_aSatellite[1].Polarisation[3].Oscillator = 10678000;	// 垂直偏波時LNB周波数
		m_aSatellite[1].Polarisation[3].Tone = 0;				// 垂直偏波時トーン信号
		m_aSatellite[1].Polarisation[4].Oscillator = 10678000;	// 水平偏波時LNB周波数
		m_aSatellite[1].Polarisation[4].Tone = 0;				// 水平偏波時トーン信号
		break;

	case 3:
		// UHF/CATVは衛星設定不要
		break;
	}

	// 衛星設定1～4の設定を読込
	for (int satellite = 1; satellite < MAX_SATELLITE; satellite++) {
		WCHAR keyname[64];
		::swprintf_s(keyname, 64, L"Satellite%01dName", satellite);
		::GetPrivateProfileStringW(L"SATELLITE", keyname, L"", buf, 64, m_szIniFilePath);
		if (buf[0] != 0) {
			m_sSatelliteName[satellite] = buf;
		}

		// 偏波種類1～4のアンテナ設定を読込
		for (int polarisation = 1; polarisation < POLARISATION_SIZE; polarisation++) {
			// 局発周波数 (KHz)
			// 全偏波共通での設定があれば読み込む
			::swprintf_s(keyname, 64, L"Satellite%01dOscillator", satellite);
			m_aSatellite[satellite].Polarisation[polarisation].Oscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].Oscillator, m_szIniFilePath);
			// 個別設定があれば上書きで読み込む
			::swprintf_s(keyname, 64, L"Satellite%01d%cOscillator", satellite, PolarisationChar[polarisation]);
			m_aSatellite[satellite].Polarisation[polarisation].Oscillator
				= (long)::GetPrivateProfileIntW(L"SATELLITE", keyname, m_aSatellite[satellite].Polarisation[polarisation].Oscillator, m_szIniFilePath);

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
	for (int modulation = 0; modulation < MAX_MODULATION; modulation++) {
		WCHAR keyname[64];
		// チャンネル名生成用変調方式名称
		::swprintf_s(keyname, 64, L"ModulationType%01dName", modulation);
		::GetPrivateProfileStringW(L"MODULATION", keyname, L"", buf, 64, m_szIniFilePath);
		if (buf[0] != 0) {
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
	m_bReserveUnusedCh = !!(::GetPrivateProfileIntW(L"CHANNEL", L"ReserveUnusedCh", 0, m_szIniFilePath));

	map<int, TuningSpaceData*>::iterator itSpace;
	map<int, ChData*>::iterator itCh;
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
			itSpace = m_TuningData.Spaces.insert(m_TuningData.Spaces.begin(), pair<int, TuningSpaceData*>(space, tuningSpaceData));
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
			for (int ch = 0; ch < 50; ch++) {
				itCh = itSpace->second->Channels.find(ch);
				if (itCh == itSpace->second->Channels.end()) {
					ChData *chData = new ChData();
					itCh = itSpace->second->Channels.insert(itSpace->second->Channels.begin(), pair<int, ChData*>(ch, chData));
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
			for (int ch = 0; ch < 51; ch++) {
				itCh = itSpace->second->Channels.find(ch);
				if (itCh == itSpace->second->Channels.end()) {
					ChData *chData = new ChData();
					itCh = itSpace->second->Channels.insert(itSpace->second->Channels.begin(), pair<int, ChData*>(ch, chData));
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
		//    チャンネル番号 = 衛星番号, 周波数, 偏波, 変調方式[, チャンネル名[, SID[, TSID[, ONID]]]]
		//    例: CH001 = 1, 12658, V, 0
		//      チャンネル番号: CH000～CH999で指定
		//      衛星番号: Satteliteセクションで設定した衛星番号(1～4) または 0(未指定時)
		//                      (地デジチューナー等は0を指定してください)
		//      周波数: 周波数をMHzで指定
		//                      (小数点を付けることによりKHz単位での指定が可能です)
		//      偏波: 'V' = 垂直偏波 'H' = 水平偏波 'L' = 左旋円偏波 'R' = 右旋円偏波 ' ' = 未指定
		//                      (地デジチューナー等は未指定)
		//      変調方式: Modulationセクションで設定した変調方式番号(0～3)
		//      チャンネル名: チャンネル名称
		//                      (省略した場合は 128.0E / 12658H / DVB - S のような形式で自動生成されます)
		//      SID: サービスID
		//      TSID: トランスポートストリームID
		//      ONID: オリジナルネットワークID
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
				itCh = itSpace->second->Channels.insert(itSpace->second->Channels.begin(), pair<int, ChData*>(chNum, chData));
			}

			WCHAR szSatellite[256] = L"";
			WCHAR szFrequency[256] = L"";
			WCHAR szPolarisation[256] = L"";
			WCHAR szModulationType[256] = L"";
			WCHAR szServiceName[256] = L"";
			WCHAR szSID[256] = L"";
			WCHAR szTSID[256] = L"";
			WCHAR szONID[256] = L"";
			::swscanf_s(buf, L"%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,]", szSatellite, 256, szFrequency, 256,
				szPolarisation, 256, szModulationType, 256, szServiceName, 256, szSID, 256, szTSID, 256, szONID, 256);

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
				szPolarisation[0] = 0;
			val = -1;
			for (int i = 0; i < POLARISATION_SIZE; i++) {
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

			// SID
			if (szSID[0] != 0) {
				itCh->second->SID = wcstol(szSID, NULL, 0);
			}

			// TSID
			if (szTSID[0] != 0) {
				itCh->second->TSID = wcstol(szTSID, NULL, 0);
			}

			// ONID
			if (szONID[0] != 0) {
				itCh->second->ONID = wcstol(szONID, NULL, 0);
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
	}

	// チューニング空間番号0を探す
	itSpace = m_TuningData.Spaces.find(0);
	if (itSpace == m_TuningData.Spaces.end()) {
		// ここには来ないはずだけど一応
		// 空のTuningSpaceDataをチューニング空間番号0に挿入
		TuningSpaceData *tuningSpaceData = new TuningSpaceData;
		itSpace = m_TuningData.Spaces.insert(m_TuningData.Spaces.begin(), pair<int, TuningSpaceData*>(0, tuningSpaceData));
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
			itSpace->second->Channels.insert(pair<int, ChData*>(0, chData));
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
			itSpace->second->Channels.insert(pair<int, ChData*>(1, chData));
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
			itSpace->second->Channels.insert(pair<int, ChData*>(2, chData));
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
	if (pnLock) *pnLock = 0;

	// チューナ固有 GetSignalState があれば、丸投げ
	HRESULT hr;
	if ((m_pIBdaSpecials) && (hr = m_pIBdaSpecials->GetSignalState(pnStrength, pnQuality, pnLock)) != E_NOINTERFACE) {
		// E_NOINTERFACE でなければ、固有関数があったという事なので、
		// このままリターン
		return;
	}

	if (m_pTunerDevice == NULL)
		return;

	IBDA_Topology *bdaNetTop = NULL;
	if (FAILED(hr = m_pTunerDevice->QueryInterface(__uuidof(IBDA_Topology), (void **)&bdaNetTop)))
		return;

	ULONG NodeTypes;
	ULONG NodeType[32];
	IUnknown *iNode = NULL;

	long longVal = 0;
	BYTE byteVal = 0;

	if (FAILED(hr = bdaNetTop->GetNodeTypes(&NodeTypes, 32, NodeType))) {
		OutputDebug(L"Fail to get node type.\n");
		return;
	}

	for (unsigned int i=0; i<NodeTypes; i++) {
		if (SUCCEEDED(hr = bdaNetTop->GetControlNode(0, 1, NodeType[i], &iNode))) {
			IBDA_SignalStatistics *pSigStats = NULL;
			if (SUCCEEDED(hr = iNode->QueryInterface(__uuidof(IBDA_SignalStatistics), (void **)(&pSigStats)))) {
				longVal = 0;
				if (SUCCEEDED(hr = pSigStats->get_SignalStrength(&longVal)))
					if (pnStrength) *pnStrength = (int)longVal;

				longVal = 0;
				if (SUCCEEDED(hr = pSigStats->get_SignalQuality(&longVal)))
					if (pnQuality) *pnQuality = min(max(longVal, 0), 100);

				byteVal = 0;
				if (SUCCEEDED(hr = pSigStats->get_SignalLocked(&byteVal)))
					if (pnLock) *pnLock = byteVal;

				SAFE_RELEASE(pSigStats);
			}
			SAFE_RELEASE(iNode);
		}
	}
	SAFE_RELEASE(bdaNetTop);

	return;
}

BOOL CBonTuner::LockChannel(const TuningParam *pTuningParam)
{
	HRESULT hr;

	// チューナ固有 LockChannel があれば、丸投げ
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->LockChannel(pTuningParam)) != E_NOINTERFACE) {
		// BonDriver_BDA改専用DLL
		// E_NOINTERFACE でなければ、固有関数があったという事なので、
		// その中で選局処理が行なわれているはず。よってこのままリターン
		m_nCurTone = pTuningParam->Antenna->Tone;
		if (SUCCEEDED(hr) && m_bLockTwice) {
			OutputDebug(L"TwiceLock 1st[Special2] SUCCESS.\n");
			::Sleep(m_nLockTwiceDelay);
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
		if (SUCCEEDED(hr) && m_bLockTwice) {
			OutputDebug(L"TwiceLock 1st[Special] SUCCESS.\n");
			::Sleep(m_nLockTwiceDelay);
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

	{
		CComQIPtr<IDVBSTuningSpace> pIDVBSTuningSpace(m_pITuningSpace);
		if (pIDVBSTuningSpace) {
			// m_pITuningSpaceの実体がDVB-S以外の時は実行されない

			// LNB 周波数を設定
			pIDVBSTuningSpace->put_HighOscillator(pTuningParam->Antenna->Oscillator);
			pIDVBSTuningSpace->put_LowOscillator(pTuningParam->Antenna->Oscillator);

			// LNBスイッチの周波数を設定
			// 10GHzを設定しておけばHigh側に、20GHzを設定しておけばLow側に切替わるはず
			pIDVBSTuningSpace->put_LNBSwitch((pTuningParam->Antenna->Tone != 0) ? 10000000 : 20000000);

			// 位相変調スペクトル反転の種類
			pIDVBSTuningSpace->put_SpectralInversion(BDA_SPECTRAL_INVERSION_AUTOMATIC);
		}
	}

	// チューナ固有トーン制御関数があれば、それをここで呼び出す
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->Set22KHz(pTuningParam->Antenna->Tone)) != E_NOINTERFACE) {
		// BonDriver_BDA改専用DLL
		if (SUCCEEDED(hr)) {
			OutputDebug(L"Set22KHz[Special2] successfully.\n");
			if (pTuningParam->Antenna->Tone != m_nCurTone) {
				m_nCurTone = pTuningParam->Antenna->Tone;
				::Sleep(m_nToneWait); // 衛星切替待ち
			}
		} else {
			OutputDebug(L"Set22KHz[Special2] failed.\n");
			// BDA generic な方法で切り替わるかもしれないので、メッセージだけ出して、そのまま続行
		}
	} else if (m_pIBdaSpecials && (hr = m_pIBdaSpecials->Set22KHz(pTuningParam->Antenna->Tone ? 1 : 0)) != E_NOINTERFACE) {
		// BonDriver_BDAオリジナル互換DLL
		if (SUCCEEDED(hr)) {
			OutputDebug(L"Set22KHz[Special] successfully.\n");
			if (pTuningParam->Antenna->Tone != m_nCurTone) {
				m_nCurTone = pTuningParam->Antenna->Tone;
				::Sleep(m_nToneWait); // 衛星切替待ち
			}
		} else {
			OutputDebug(L"Set22KHz[Special] failed.\n");
			// BDA generic な方法で切り替わるかもしれないので、メッセージだけ出して、そのまま続行
		}
	} else {
		// 固有関数がないだけなので、何もせず
	}

	CComQIPtr<ITuner> pITuner(m_pNetworkProvider);
	if (!pITuner) {
		OutputDebug(L"Fail to get ITuner.\n");
		return FALSE;
	}

	CComPtr<ITuneRequest> pITuneRequest;
	if (FAILED(hr = m_pITuningSpace->CreateTuneRequest(&pITuneRequest))) {
		OutputDebug(L"Fail to create ITuneRequest.\n");
		return FALSE;
	}

	{
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

		{
			CComQIPtr<IDVBSLocator> pIDVBSLocator(pILocator);
			if (pIDVBSLocator) {
				// m_pITuningSpaceの実体がDVB-S以外の時は実行されない

				// 信号の偏波を設定
				pIDVBSLocator->put_SignalPolarisation(pTuningParam->Polarisation);
			}
		}

		{
			CComQIPtr<IDVBTLocator> pIDVBTLocator(pILocator);
			if (pIDVBTLocator) {
				// m_pITuningSpaceの実体がDVB-T以外の時は実行されない

				// 周波数の帯域幅 (MHz)を設定
				pIDVBTLocator->put_Bandwidth(6);
			}
		}

		pITuneRequest->put_Locator(pILocator);

		// DVB Triplet IDの設定
		{
			CComQIPtr<IDVBTuneRequest> pIDVBTuneRequest(pITuneRequest);
			if (pITuneRequest) {
				if (pTuningParam->ONID != -1)
					pIDVBTuneRequest->put_ONID(pTuningParam->ONID);
				if (pTuningParam->TSID != -1)
					pIDVBTuneRequest->put_TSID(pTuningParam->TSID);
				if (pTuningParam->SID != -1)
					pIDVBTuneRequest->put_SID(pTuningParam->SID);
			}
		}
	}

	if (m_pIBdaSpecials2) {
		// m_pIBdaSpecialsでput_TuneRequestの前に何らかの処理が必要なら行う
		hr = m_pIBdaSpecials2->PreTuneRequest(pTuningParam, pITuneRequest);
	}

	if (pTuningParam->Antenna->Tone != m_nCurTone) {
		//トーン切替ありの場合、先に一度TuneRequestしておく
		OutputDebug(L"Requesting pre tune.\n");
		if (FAILED(hr = pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"Fail to put pre tune request.\n");
			return FALSE;
		}
		OutputDebug(L"Pre tune request complete.\n");

		m_nCurTone = pTuningParam->Antenna->Tone;
		::Sleep(m_nToneWait); // 衛星切替待ち
	}

	if (m_bLockTwice) {
		// TuneRequestを強制的に2度行う
		OutputDebug(L"Requesting 1st twice tune.\n");
		if (FAILED(hr = pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"Fail to put 1st twice tune request.\n");
			return FALSE;
		}
		OutputDebug(L"1st Twice tune request complete.\n");
		::Sleep(m_nLockTwiceDelay);
	}

	unsigned int nRetryRemain = m_nLockWaitRetry;
	int nLock = 0;
	do {
		OutputDebug(L"Requesting tune.\n");
		if (FAILED(hr = pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"Fail to put tune request.\n");
			return FALSE;
		}
		OutputDebug(L"Tune request complete.\n");

		static const int LockRetryTime = 50;
		unsigned int nWaitRemain = m_nLockWait;
		::Sleep(m_nLockWaitDelay);
		GetSignalState(NULL, NULL, &nLock);
		while (!nLock && nWaitRemain) {
			DWORD dwSleepTime = (nWaitRemain > LockRetryTime) ? LockRetryTime : nWaitRemain;
			OutputDebug(L"Waiting lock status after %d msec.\n", nWaitRemain);
			::Sleep(dwSleepTime);
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
HRESULT CBonTuner::CheckAndInitTunerDependDll(void)
{
	if (m_aTunerParam.sDLLBaseName == L"") {
		// チューナ固有関数を使わない場合
		return S_OK;
	}

	if (m_aTunerParam.sDLLBaseName == L"AUTO") {
		// INI ファイルで "AUTO" 指定の場合
		BOOL found = FALSE;
		for (int i = 0; i < sizeof aTunerSpecialData / sizeof TUNER_SPECIAL_DLL; i++) {
			if ((aTunerSpecialData[i].sTunerGUID != L"") && (m_sTunerDisplayName.find(aTunerSpecialData[i].sTunerGUID)) != wstring::npos) {
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

	return (*func)(m_pTunerDevice, m_sTunerDisplayName.c_str(), m_sTunerFriendlyName.c_str(), m_szIniFilePath);
}

// チューナ固有Dllでのキャプチャデバイス確認
HRESULT CBonTuner::CheckCapture(wstring displayName, wstring friendlyName)
{
	if (m_hModuleTunerSpecials == NULL) {
		return S_OK;
	}

	HRESULT(*func)(const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*) =
		(HRESULT(*)(const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*))::GetProcAddress(m_hModuleTunerSpecials, "CheckCapture");
	if (!func) {
		return S_OK;
	}

	return (*func)(m_sTunerDisplayName.c_str(), m_sTunerFriendlyName.c_str(), displayName.c_str(), friendlyName.c_str(), m_szIniFilePath);
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

	m_pIBdaSpecials2 = dynamic_cast<IBdaSpecials2a *>(m_pIBdaSpecials);
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
	HRESULT hr;

	// create tuning space
	IID iidTuningSpace;
	IID iidITuningSpace;
	IID iidLocator;
	IID iidILocator;
	IID iidNetworkType;
	DVBSystemType dvbSystemType;

	if (m_nDVBSystemType == eTunerTypeDVBT) {
		iidTuningSpace = __uuidof(DVBTuningSpace);
		iidITuningSpace = __uuidof(IDVBTuningSpace2);
		iidLocator = __uuidof(DVBTLocator);
		iidILocator = __uuidof(IDVBTLocator);
		iidNetworkType = CLSID_DVBTNetworkProvider;
		dvbSystemType = DVB_Terrestrial;
	}
	else {
		iidTuningSpace = __uuidof(DVBSTuningSpace);
		iidITuningSpace = __uuidof(IDVBSTuningSpace);
		iidLocator = __uuidof(DVBSLocator);
		iidILocator = __uuidof(IDVBSLocator);
		iidNetworkType = CLSID_DVBSNetworkProvider;
		dvbSystemType = DVB_Satellite;
	}
		if (FAILED(hr = ::CoCreateInstance(iidTuningSpace, NULL, CLSCTX_INPROC_SERVER, iidITuningSpace, (void**)&m_pITuningSpace))) {
		OutputDebug(L"FAILED: CoCreateInstance(ITuningSpace)\n");
		return hr;
	}
	if (!m_pITuningSpace) {
		OutputDebug(L"Failed to get DVBSTuningSpace\n");
		return E_FAIL;
	}

	{
		CComQIPtr<IDVBTuningSpace> pIDVBTuningSpace(m_pITuningSpace);
		if (pIDVBTuningSpace)
		{
			// set system type
			pIDVBTuningSpace->put_SystemType(dvbSystemType);

			// set network type
			if (FAILED(hr = pIDVBTuningSpace->put__NetworkType(iidNetworkType))) {
				OutputDebug(L"put_NetworkType failed\n");
				return hr;
			}
		}
	}

	// set the Frequency mapping
	_bstr_t bstrFreqMapping(L"");
	hr = m_pITuningSpace->put_FrequencyMapping(bstrFreqMapping);

	{
		// Set default locator
		CComPtr<ILocator> pILocator;
		if (FAILED(hr = pILocator.CoCreateInstance(iidLocator)) || !pILocator) {
			OutputDebug(L"Fail to get ILocator.\n");
			return FALSE;
		}

		pILocator->put_CarrierFrequency(-1);
		pILocator->put_SymbolRate(-1);
		pILocator->put_InnerFEC(BDA_FEC_METHOD_NOT_SET);
		pILocator->put_InnerFECRate(BDA_BCC_RATE_NOT_SET);
		pILocator->put_OuterFEC(BDA_FEC_METHOD_NOT_SET);
		pILocator->put_OuterFECRate(BDA_BCC_RATE_NOT_SET);
		pILocator->put_Modulation(BDA_MOD_NOT_SET);

		{
			CComQIPtr<IDVBSLocator> pIDVBSLocator(pILocator);
			if (pIDVBSLocator) {
				pIDVBSLocator->put_Modulation(BDA_MOD_NOT_SET);
				pIDVBSLocator->put_WestPosition(FALSE);
				pIDVBSLocator->put_OrbitalPosition(-1);
				pIDVBSLocator->put_Elevation(-1);
				pIDVBSLocator->put_Azimuth(-1);
				pIDVBSLocator->put_SignalPolarisation(BDA_POLARISATION_NOT_SET);
			}
		}

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

		m_pITuningSpace->put_DefaultLocator(pILocator);
	}
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
	CComQIPtr<ITuner> pITuner(m_pNetworkProvider);
	if (!pITuner) {
		OutputDebug(L"Fail to get ITuner.\n");
		return E_FAIL;
	}
	pITuner->put_TuningSpace(m_pITuningSpace);

	HRESULT hr;
	CComPtr<ITuneRequest> pITuneRequest;
	if (FAILED(hr = m_pITuningSpace->CreateTuneRequest(&pITuneRequest))) {
		OutputDebug(L"Fail to create ITuneRequest.\n");
		return hr;
	}

	CComPtr<ILocator> pILocator;
	if (FAILED(hr = m_pITuningSpace->get_DefaultLocator(&pILocator))) {
		OutputDebug(L"Fail to get ILocator.\n");
		return hr;
	}

	pITuneRequest->put_Locator(pILocator);
	pITuner->put_TuneRequest(pITuneRequest);

	return S_OK;
}

HRESULT CBonTuner::LoadNetworkProvider(void)
{
	HRESULT hr;
	if (!m_pITuningSpace) {
		OutputDebug(L"TuningSpace NOT SET.\n");
		return E_POINTER;
	}

	CLSID CLSIDNetworkType;
	if (FAILED(hr = m_pITuningSpace->get__NetworkType(&CLSIDNetworkType))) {
		OutputDebug(L"Fail to get network type.\n");
		return hr;
	}

	if (FAILED(hr = ::CoCreateInstance(CLSIDNetworkType, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)(&m_pNetworkProvider)))) {
		OutputDebug(L"Fail to create network-provider.\n");
		return hr;
	}

	if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pNetworkProvider, FILTER_GRAPH_NAME_NETWORK_PROVIDER))) {
		OutputDebug(L"Fail to add network-provider into graph.\n");
		SAFE_RELEASE(m_pNetworkProvider);
		return hr;
	}

	return S_OK;
}

void CBonTuner::UnloadNetworkProvider(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pNetworkProvider)
		hr = m_pIGraphBuilder->RemoveFilter(m_pNetworkProvider);

	SAFE_RELEASE(m_pNetworkProvider);
}

// Load tuner device
//
// ini ファイルでチューナが特定されている場合はそれをロードする。
// 指定されてなければ KSCATEGORY_BDA_NETWORK_TUNER カテゴリのチューナを
// 順番にロードして NetworkProvider とつながる物を見つける
HRESULT CBonTuner::LoadAndConnectTunerDevice(void)
{
	HRESULT hr;
	if (!m_pITuningSpace || !m_pNetworkProvider) {
		OutputDebug(L"[P->T] TuningSpace or NetworkProvider NOT SET.\n");
		return E_POINTER;
	}

	try {
		CDSFilterEnum dsfEnum(KSCATEGORY_BDA_NETWORK_TUNER, CDEF_DEVMON_PNP_DEVICE);
		while (SUCCEEDED(hr = dsfEnum.next()) && hr == S_OK) {
			// チューナの DisplayName, FriendlyName を得る
			dsfEnum.getDisplayName(&m_sTunerDisplayName);
			dsfEnum.getFriendlyName(&m_sTunerFriendlyName);

			// iniファイルでチューナを指定している場合
			// DisplayName に GUID が含まれるか検査して、NOだったら次のチューナへ
			bool found = true;
			for (int i = 0; i < MAX_GUID; i++) {
				if (m_aTunerParam.sTunerGUID[i].compare(L"") == 0)
					continue;
				found = false;
				if (m_sTunerDisplayName.find(m_aTunerParam.sTunerGUID[i]) != wstring::npos) {
					found = true;
					break;
				}
			}
			if (!found)
				continue;
			// FriendlyName が含まれるか検査して、NOだったら次のチューナへ
			for (int i = 0; i < MAX_GUID; i++) {
				if (m_aTunerParam.sTunerFriendlyName[i].compare(L"") == 0)
					continue;
				found = false;
				if (m_sTunerFriendlyName.find(m_aTunerParam.sTunerFriendlyName[i]) != wstring::npos) {
					found = true;
					break;
				}
			}
			if (!found)
				continue;
			OutputDebug(L"[P->T] Trying tuner device=FriendlyName:%s\n  GUID:%s\n", m_sTunerFriendlyName.c_str(), m_sTunerDisplayName.c_str());

			// 排他処理用にセマフォ用文字列を作成 ('\' -> '/')
			wstring::size_type n = 0;
			wstring semName = m_sTunerDisplayName;
			while ((n = semName.find(L'\\', n)) != wstring::npos) {
				semName.replace(n, 1, 1, L'/');
			}
			semName = L"Global\\" + semName;
			
			// 排他処理
			m_hSemaphore = ::CreateSemaphoreW(NULL, 1, 1, semName.c_str());
			DWORD result = ::WaitForSingleObject(m_hSemaphore, 0);
			if (result != WAIT_OBJECT_0) {
				OutputDebug(L"[P->T] Another is using.\n");
				// 使用中なので次のチューナを探す
				::CloseHandle(m_hSemaphore);
				m_hSemaphore = NULL;
				SAFE_RELEASE(m_pTunerDevice);
				continue;
			}
				
			if (SUCCEEDED(hr = dsfEnum.getFilter(&m_pTunerDevice))) {
				if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pTunerDevice, m_sTunerFriendlyName.c_str()))) {
					OutputDebug(L"[P->T] Error in AddFilter\n");
					::ReleaseSemaphore(m_hSemaphore, 1, NULL);
					::CloseHandle(m_hSemaphore);
					m_hSemaphore = NULL;
					SAFE_RELEASE(m_pTunerDevice);
					continue;
				}
				
				// connect してみる
				if (SUCCEEDED(hr = Connect(L"Provider->Tuner", m_pNetworkProvider, m_pTunerDevice))) {
					// connect 成功
					OutputDebug(L"[P->T] Connect OK.\n");

					// チューナ固有Dllが必要なら読込み、固有の初期化処理があれば呼び出す
					if (FAILED(hr = CheckAndInitTunerDependDll())) {
						// 何らかの理由で使用できないみたいなので次のチューナへ
						OutputDebug(L"[P->T] Discarded by BDASpecials.\n");
						ReleaseTunerDependCode();
						m_pIGraphBuilder->RemoveFilter(m_pTunerDevice);
						SAFE_RELEASE(m_pTunerDevice);
						::ReleaseSemaphore(m_hSemaphore, 1, NULL);
						::CloseHandle(m_hSemaphore);
						m_hSemaphore = NULL;
						continue;
					}

					// 使用できるCaptureとの接続～RunGraphまでを試みる
					if (FAILED(hr = LoadAndConnectCaptureDevice())) {
						// 動作する組合せが見つからなかったので次のチューナへ
						DisconnectAll(m_pTunerDevice);
						UnloadCaptureDevice();
						ReleaseTunerDependCode();
						m_pIGraphBuilder->RemoveFilter(m_pTunerDevice);
						SAFE_RELEASE(m_pTunerDevice);
						::ReleaseSemaphore(m_hSemaphore, 1, NULL);
						::CloseHandle(m_hSemaphore);
						m_hSemaphore = NULL;
						continue;
					}
					// 成功
					// ここでチューナが確定するので、チューナ固有関数をロードする
					LoadTunerDependCode();
					return S_OK;
				} else {
					// NetworkProviderが異なる等の理由でconnectに失敗
					// 次のチューナへ
					OutputDebug(L"[P->T] Connect Failed.\n");
					m_pIGraphBuilder->RemoveFilter(m_pTunerDevice);
					SAFE_RELEASE(m_pTunerDevice);
					::ReleaseSemaphore(m_hSemaphore, 1, NULL);
					::CloseHandle(m_hSemaphore);
					m_hSemaphore = NULL;
				}
			} else {
				::ReleaseSemaphore(m_hSemaphore, 1, NULL);
				::CloseHandle(m_hSemaphore);
				m_hSemaphore = NULL;
			}
		}
		OutputDebug(L"[P->T] Tuner not found.\n");
		return E_FAIL;
	} catch (...) {
		OutputDebug(L"[P->T] Fail to construct CDSFilterEnum.\n");
		SAFE_RELEASE(m_pTunerDevice);
		return E_FAIL;
	}
}

void CBonTuner::UnloadTunerDevice(void)
{
	HRESULT hr;

	ReleaseTunerDependCode();

	if (m_pIGraphBuilder && m_pTunerDevice)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTunerDevice);

	SAFE_RELEASE(m_pTunerDevice);
}

// Load capture device
//
// ini ファイルでキャプチャデバイスが特定されている場合はそれをロードする。
// 指定されてなければ KSCATEGORY_BDA_RECEIVER_COMPONENT カテゴリのCaptureDeviceを
// 順番にロードして Load済みの TunerDevice と connect できる CaptureDevice を
// 探して connect する
HRESULT CBonTuner::LoadAndConnectCaptureDevice(void)
{
	HRESULT hr;

	if (!m_pITuningSpace || !m_pNetworkProvider || ! m_pTunerDevice) {
		OutputDebug(L"[T->C] TuningSpace, NetworkProvider or TunerDevice NOT SET.\n");
		return E_POINTER;
	}

	try {
		CDSFilterEnum dsfEnum(KSCATEGORY_BDA_RECEIVER_COMPONENT, CDEF_DEVMON_PNP_DEVICE);
		while (SUCCEEDED(hr = dsfEnum.next()) && hr == S_OK) {
			// キャプチャデバイスの DisplayName, FriendlyName を得る
			wstring displayName;
			wstring friendlyName;
			dsfEnum.getDisplayName(&displayName);
			dsfEnum.getFriendlyName(&friendlyName);

			// iniファイルでキャプチャデバイスを指定している場合
			// DisplayName に GUID が含まれるか検査して、NOだったら次のキャプチャデバイスへ
			bool found = true;
			for (int i = 0; i < MAX_GUID; i++) {
				if (m_aTunerParam.sCaptureGUID[i].compare(L"") == 0)
					continue;
				found = false;
				if (displayName.find(m_aTunerParam.sCaptureGUID[i]) != wstring::npos) {
					found = true;
					break;
				}
			}
			if (!found)
				continue;
			// FriendlyName が含まれるか検査して、NOだったら次のキャプチャデバイスへ
			for (int i = 0; i < MAX_GUID; i++) {
				if (m_aTunerParam.sCaptureFriendlyName[i].compare(L"") == 0)
					continue;
				found = false;
				if (friendlyName.find(m_aTunerParam.sCaptureFriendlyName[i]) != wstring::npos) {
					found = true;
					break;
				}
			}
			if (!found)
				continue;
			OutputDebug(L"[T->C] Trying capture device=FriendlyName:%s\n  GUID:%s\n", friendlyName.c_str(), displayName.c_str());

			if (m_aTunerParam.bCheckDeviceInstancePath) {
				// チューナデバイスとキャプチャデバイスのデバイスインスタンスパスが一致しているか確認
				wstring::size_type n, last;
				n = last = 0;
				while ((n = m_sTunerDisplayName.find(L'#', n)) != wstring::npos) {
					last = n;
					n++;
				}
				if (last != 0) {
					wstring path = m_sTunerDisplayName.substr(0, last);
					if (displayName.find(path) == wstring::npos) {
						// デバイスパスが異なっているので次のキャプチャーデバイスへ
						OutputDebug(L"[T->C] Capture device instance path not match.\n");
						continue;
					}
				}
			}

			// チューナ固有Dllでの確認処理があれば呼び出す
			if (FAILED(hr = CheckCapture(displayName, friendlyName))) {
				// 固有Dllがダメと言っているので次のチューナデバイスへ
				OutputDebug(L"[T->C] Discarded by BDASpecials.\n");
				continue;
			}

			if (SUCCEEDED(hr = dsfEnum.getFilter(&m_pCaptureDevice))) {
				if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pCaptureDevice, friendlyName.c_str()))) {
					OutputDebug(L"[T->C] Error in AddFilter.\n");
					SAFE_RELEASE(m_pCaptureDevice);
					continue;
				}

				// connect してみる
				if (SUCCEEDED(hr = Connect(L"Tuner->Capture", m_pTunerDevice, m_pCaptureDevice))) {
					// connect 成功
					OutputDebug(L"[T->C] Connect OK.\n");
					// TsWriteと接続
					if (FAILED(LoadAndConnectTsWriter())) {
						// 失敗したら次のキャプチャデバイスへ
						DisconnectAll(m_pTsWriter);
						DisconnectAll(m_pCaptureDevice);
						UnloadTsWriter();
						m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);
						SAFE_RELEASE(m_pCaptureDevice);
						continue;
					}
					// TsDemuxerと接続
					if (FAILED(LoadAndConnectDemux())) {
						// 失敗したら次のキャプチャデバイスへ
						DisconnectAll(m_pDemux);
						DisconnectAll(m_pTsWriter);
						DisconnectAll(m_pCaptureDevice);
						UnloadDemux();
						UnloadTsWriter();
						m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);
						SAFE_RELEASE(m_pCaptureDevice);
						continue;
					}
					// TIFと接続
					if (FAILED(LoadAndConnectTif())) {
						// 失敗したら次のキャプチャデバイスへ
						DisconnectAll(m_pTif);
						DisconnectAll(m_pDemux);
						DisconnectAll(m_pTsWriter);
						DisconnectAll(m_pCaptureDevice);
						UnloadTif();
						UnloadDemux();
						UnloadTsWriter();
						m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);
						SAFE_RELEASE(m_pCaptureDevice);
						continue;
					}
					// Runしてみる
					if (FAILED(hr = RunGraph())) {
						// 失敗したら次のキャプチャデバイスへ
						OutputDebug(L"RunGraph Failed.\n");
						DisconnectAll(m_pTif);
						DisconnectAll(m_pDemux);
						DisconnectAll(m_pTsWriter);
						DisconnectAll(m_pCaptureDevice);
						UnloadTif();
						UnloadDemux();
						UnloadTsWriter();
						m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);
						SAFE_RELEASE(m_pCaptureDevice);
						continue;
					}
					// 成功したのでそのままreturn
					OutputDebug(L"RunGraph OK.\n");
					return S_OK;
				} else {
					// connect できなければチューナとの組合せが正しくないと思われる
					// 次のキャプチャデバイスへ
					OutputDebug(L"[T->C] Connect Failed, trying next capturedevice.\n");
					m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);
					SAFE_RELEASE(m_pCaptureDevice);
				}
			}
		}
		OutputDebug(L"[T->C] CaptureDevice not found.\n");
		return E_FAIL;
	} catch (...) {
		OutputDebug(L"[T->C] Fail to construct CDSFilterEnum.\n");
		SAFE_RELEASE(m_pCaptureDevice);
		return E_FAIL;
	}

}

void CBonTuner::UnloadCaptureDevice(void)
{
	HRESULT hr;

	if (m_pIGraphBuilder && m_pCaptureDevice)
		hr = m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);

	SAFE_RELEASE(m_pCaptureDevice);
}

HRESULT CBonTuner::LoadAndConnectTsWriter(void)
{
	HRESULT hr = E_FAIL;

	if (!m_pITuningSpace || !m_pNetworkProvider || !m_pTunerDevice || !m_pCaptureDevice) {
		OutputDebug(L"[C->W] TuningSpace, NetworkProvider, TunerDevice or CaptureDevice NOT SET.\n");
		return E_POINTER;
	}

	m_pCTsWriter = static_cast<CTsWriter *>(CTsWriter::CreateInstance(NULL, &hr));
	if (!m_pCTsWriter) {
		OutputDebug(L"[C->W] Fail to load TsWriter filter.\n");
		return E_NOINTERFACE;
	}

	m_pCTsWriter->AddRef();

	if (FAILED(hr = m_pCTsWriter->QueryInterface(IID_IBaseFilter, (void**)(&m_pTsWriter)))) {
		OutputDebug(L"[C->W] Fail to get TsWriter interface.\n");
		SAFE_RELEASE(m_pCTsWriter);
		return hr;
	}

	if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pTsWriter, FILTER_GRAPH_NAME_TSWRITER))) {
		OutputDebug(L"[C->W] Fail to add TsWriter filter into graph.\n");
		SAFE_RELEASE(m_pTsWriter);
		SAFE_RELEASE(m_pCTsWriter);
		return hr;
	}

	if (FAILED(hr = Connect(L"Capture->TsWriter", m_pCaptureDevice, m_pTsWriter))) {
		OutputDebug(L"[C->W] Failed to connect.\n");
		SAFE_RELEASE(m_pTsWriter);
		SAFE_RELEASE(m_pCTsWriter);
		return hr;
	}

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

	if (!m_pITuningSpace || !m_pNetworkProvider || !m_pTunerDevice || !m_pCaptureDevice || !m_pTsWriter) {
			OutputDebug(L"[W->M] TuningSpace, NetworkProvider, TunerDevice CaptureDevice or Grabber NOT SET.\n");
			return E_POINTER;
	}

	if (FAILED(hr = ::CoCreateInstance(CLSID_MPEG2Demultiplexer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)(&m_pDemux)))) {
		OutputDebug(L"[W->M] Fail to load MPEG2-Demultiplexer.\n");
		return hr;
	}

	if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pDemux, FILTER_GRAPH_NAME_DEMUX))) {
		OutputDebug(L"[W->M] Fail to add MPEG2-Demultiplexer into graph.\n");
		SAFE_RELEASE(m_pDemux);
		return hr;
	}

	if (FAILED(hr = Connect(L"Grabber->Demux", m_pTsWriter, m_pDemux))) {
		OutputDebug(L"[W->M] Fail to connect Grabber->Demux.\n");
		SAFE_RELEASE(m_pDemux);
		return hr;
	}

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

	if (!m_pITuningSpace || !m_pNetworkProvider || !m_pTunerDevice || !m_pCaptureDevice || !m_pTsWriter || !m_pDemux) {
			OutputDebug(L"[M->I] TuningSpace, NetworkProvider, TunerDevice CaptureDevice, Grabber or Demux NOT SET.\n");
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

			if (SUCCEEDED(hr = dsfEnum.getFilter(&m_pTif))) {
				if (FAILED(hr = m_pIGraphBuilder->AddFilter(m_pTif, FILTER_GRAPH_NAME_TIF))) {
					SAFE_RELEASE(m_pTif);
					OutputDebug(L"[M->I] Error in AddFilter.\n");
					return hr;
				}
				// connect してみる
				if (SUCCEEDED(hr = Connect(L"Demux -> Tif", m_pDemux, m_pTif))) {
					OutputDebug(L"[M->I] Connect OK.\n");
					// connect 成功 なのでこのまま終了
					return S_OK;
				} else {
					m_pIGraphBuilder->RemoveFilter(m_pTif);
					SAFE_RELEASE(m_pTif);
					// connect できなければ次の TIF フィルタへ
				}
			}
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
