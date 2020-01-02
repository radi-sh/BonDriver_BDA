//------------------------------------------------------------------------------
// File: BonTuner.h
//   Define CBonTuner class
//------------------------------------------------------------------------------
#pragma once

#include "common.h"

#include <Windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <atlbase.h>		// CComPtr
#include <strmif.h>
#include <tuner.h>

#include "IBonDriver2.h"
#include "LockChannel.h"
#include "TunerComboList.h"
#include "TSMF.h"
#include "EnumSettingValue.h"

#pragma warning (push)
#pragma warning (disable: 4310)
#include "..\3rdParties\muparser\include\muParser.h"
#pragma warning (pop)

#pragma comment(lib, "muparser.lib")

class TS_DATA;
class TS_BUFF;
class CTSMFParser;
class CBitRate;
class CCOMProc;
class CDecodeProc;
class IBdaSpecials;
class IBdaSpecials2b5;

struct IMediaControl;
struct ITsWriter;

// CBonTuner class
////////////////////////////////
class CBonTuner : public IBonDriver2
{
public:
	////////////////////////////////////////
	// コンストラクタ & デストラクタ
	////////////////////////////////////////
	CBonTuner();
	virtual ~CBonTuner();

	////////////////////////////////////////
	// IBonDriver メンバ関数
	////////////////////////////////////////
	const BOOL OpenTuner(void);
	void CloseTuner(void);

	const BOOL SetChannel(const BYTE byCh);
	const float GetSignalLevel(void);

	const DWORD WaitTsStream(const DWORD dwTimeOut = 0);
	const DWORD GetReadyCount(void);

	const BOOL GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain);
	const BOOL GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain);

	void PurgeTsStream(void);

	////////////////////////////////////////
	// IBonDriver2 メンバ関数
	////////////////////////////////////////
	LPCTSTR GetTunerName(void);

	const BOOL IsTunerOpening(void);

	LPCTSTR EnumTuningSpace(const DWORD dwSpace);
	LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel);

	const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel);

	const DWORD GetCurSpace(void);
	const DWORD GetCurChannel(void);

	void Release(void);
	
	////////////////////////////////////////
	// 静的メンバ関数
	////////////////////////////////////////

	// 必要な静的変数初期化
	static void Init(HMODULE hModule);

	// 静的変数の解放
	static void Finalize(void);

	////////////////////////////////////////
	// 静的メンバ変数
	////////////////////////////////////////

	// Dllのモジュールハンドル
	static HMODULE st_hModule;

	// 作成されたCBontunerインスタンスの一覧
	static std::list<CBonTuner*> st_InstanceList;

	// st_InstanceList操作用
	static CRITICAL_SECTION st_LockInstanceList;

protected:
	////////////////////////////////////////
	// 内部メンバ関数
	////////////////////////////////////////

	// COM処理専用スレッド
	static DWORD WINAPI COMProcThread(LPVOID lpParameter);

	// Decode処理専用スレッド
	static DWORD WINAPI DecodeProcThread(LPVOID lpParameter);

	// TsWriter コールバック関数
	static int CALLBACK RecvProc(void* pParam, BYTE* pbData, size_t size);

	// データ受信スタート・停止
	void StartRecv(void);
	void StopRecv(void);

	// ini ファイル読込
	void ReadIniFile(void);

	// 信号状態を取得
	HRESULT GetSignalState(double* pdbStrength, double* pdbQuality, double* pdbLocked, double* pdbPresent);

	// チャンネル切替
	BOOL LockChannel(const TuningParam *pTuningParam, BOOL bLockTwice);

	// チューナ固有Dllのロード
	HRESULT CheckAndInitTunerDependDll(IBaseFilter * pTunerDevice, std::wstring tunerGUID, std::wstring tunerFriendlyName);

	// チューナ固有Dllでのキャプチャデバイス確認
	HRESULT CheckCapture(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName);
		
	// チューナ固有関数のロード
	void LoadTunerDependCode(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName);

	// チューナ固有関数とDllの解放
	void ReleaseTunerDependCode(void);

	// GraphBuilder
	HRESULT InitializeGraphBuilder(void);
	void CleanupGraph(void);
	HRESULT RunGraph(void);
	void StopGraph(void);

	struct DVBSystemTypeData;

	// TuningSpace
	HRESULT CreateTuningSpace(DVBSystemTypeData* pDVBSystemTypeData);
	void UnloadTuningSpace(void);
	HRESULT InitTuningSpace(void);

	// NetworkProvider
	HRESULT LoadNetworkProvider(DVBSystemTypeData* pDVBSystemTypeData);
	void UnloadNetworkProvider(void);

	// チューナ・キャプチャデバイスを含めてすべてのフィルタグラフをロードしてRunを試みる
	HRESULT LoadAndConnectDevice(unsigned int tunerGroup);

	// TunerDevice
	void UnloadTunerDevice(void);
	
	// CaptureDevice
	void UnloadCaptureDevice(void);
	
	// TsWriter
	HRESULT LoadAndConnectTsWriter(IBaseFilter* pTunerDevice, IBaseFilter* pCaptureDevice);
	void UnloadTsWriter(void);

	// Demultiplexer
	HRESULT LoadAndConnectDemux(void);
	void UnloadDemux(void);
	
	// TIF (Transport Information Filter)
	HRESULT LoadAndConnectTif(void);
	void UnloadTif(void);

	// TsWriter/Demultiplexer/TIFをLoad&ConnectしRunする
	HRESULT LoadAndConnectMiscFilters(IBaseFilter* pTunerDevice, IBaseFilter* pCaptureDevice);

	// チューナ信号状態取得用インターフェース
	HRESULT LoadTunerSignalStatisticsTunerNode(void);
	HRESULT LoadTunerSignalStatisticsDemodNode(void);
	void UnloadTunerSignalStatistics(void);

	// Pin の接続
	HRESULT Connect(IBaseFilter* pFrom, IBaseFilter* pTo);

	// 全ての Pin を切断する
	void DisconnectAll(IBaseFilter* pFilter);

	// CCOM処理専用スレッドから呼び出される関数
	BOOL _OpenTuner(void);
	void _CloseTuner(BOOL putoff);
	BOOL _SetChannel(const DWORD dwSpace, const DWORD dwChannel);
	float _GetSignalLevel(void);
	BOOL _IsTunerOpening(void);
	DWORD _GetCurSpace(void);
	DWORD _GetCurChannel(void);


protected:
	////////////////////////////////////////
	// メンバ変数
	////////////////////////////////////////

	// COM処理専用スレッド用
	CCOMProc* m_pCOMProc = NULL;

	// Decode処理専用スレッド用
	CDecodeProc* m_pDecodeProc = NULL;
	BOOL m_bNeedDecodeProc = FALSE;

	////////////////////////////////////////
	// チューナパラメータ関係
	////////////////////////////////////////

	// INI ファイルで指定できるTunerGroup最大数
	static constexpr unsigned int MAX_TUNER_GROUP = 10U;

	// INIファイルで指定できるGUID/FriendlyName最大数
	static constexpr unsigned int MAX_GUID = 100U;

	// チューナ・キャプチャデバイスの読込みリスト取得クラス
	CTunerComboList m_TunerComboList;

	// GetTunerNameで返す名前
	std::basic_string<TCHAR> m_sTunerName = L"DVB-S2";

	// 固有DLL名
	std::wstring m_sDLLBaseName;

	// Tone信号切替時のWait時間
	unsigned int m_nToneWait = 0;

	// CH切替後のLock確認時間
	unsigned int m_nLockWait = 2000;

	// CH切替後のLock確認Delay時間
	unsigned int m_nLockWaitDelay = 0;

	// CH切替後のLock確認Retry回数
	unsigned int m_nLockWaitRetry = 0;

	// CH切替動作を強制的に2度行うかどうか
	BOOL m_bLockTwice = FALSE;

	// CH切替動作を強制的に2度行う場合のDelay時間(msec)
	unsigned int m_nLockTwiceDelay = 100;

	// SignalLockedの監視時間(msec) 0で監視しない
	unsigned int m_nWatchDogSignalLocked = 0;

	// BitRateの監視時間(msec) 0で監視しない
	unsigned int m_nWatchDogBitRate = 0;

	// 異常検知時、チューナの再オープンを試みるまでのCH切替動作試行回数
	unsigned int m_nReOpenWhenGiveUpReLock = 0;

	// チューナの再オープンを試みる場合に別のチューナを優先して検索するかどうか
	BOOL m_bTryAnotherTuner = FALSE;

	// CH切替に失敗した場合に、異常検知時同様バックグランドでCH切替動作を行うかどうか
	BOOL m_bBackgroundChannelLock = FALSE;

	// SignalLevel 算出方法
	EnumSettingValue::SignalLevelCalcType m_nSignalLevelCalcType = EnumSettingValue::SignalLevelCalcType::SSStrength;
	BOOL m_bSignalLevelGetTypeSS = FALSE;			// SignalLevel 算出に RF Tuner Node の IBDA_SignalStatistics を使用する
	BOOL m_bSignalLevelGetTypeTuner = FALSE;		// SignalLevel 算出に ITuner を使用する
	BOOL m_bSignalLevelGetTypeDemodSS = FALSE;		// SignalLevel 算出に Demodulator Node の IBDA_SignalStatistics を使用する
	BOOL m_bSignalLevelGetTypeBR= FALSE;			// SignalLevel 算出に ビットレート値を使用する
	BOOL m_bSignalLevelNeedStrength = FALSE;		// SignalLevel 算出に SignalStrength 値を使用する
	BOOL m_bSignalLevelNeedQuality = FALSE;			// SignalLevel 算出に SignalQuality 値を使用する

	// SignalLevel算出用muparser
	mu::Parser m_muParser;

	// 現在のStrength値
	double m_dbStrength = 0.0;

	// 現在のQuality値
	double m_dbQuality = 0.0;

	// 現在のLock状態値
	double m_dbSignalLocked = 0.0;

	// 現在の信号提供状態値
	double m_dbSignalPresent = 0.0;

	// Strength 値補正係数
	double m_dbStrengthCoefficient = 1.0;

	// Quality 値補正係数
	double m_dbQualityCoefficient = 1.0;

	// Strength 値補正バイアス
	double m_dbStrengthBias = 0.0;

	// Quality 値補正バイアス
	double m_dbQualityBias = 0.0;

	// SignalLevel算出用ユーザー定義数式
	std::wstring m_sSignalLevelCalcFormula;

	// チューニング状態の判断方法
	EnumSettingValue::SignalLockedJudgeType m_nSignalLockedJudgeType = EnumSettingValue::SignalLockedJudgeType::SS;
	BOOL m_bSignalLockedJudgeTypeSS = FALSE;		// チューニング状態の判断に IBDA_SignalStatistics を使用する
	BOOL m_bSignalLockedJudgeTypeTuner = FALSE;		// チューニング状態の判断に ITuner を使用する
	BOOL m_bSignalLockedJudgeTypeDemodSS = FALSE;	// チューニング状態の判断に Demodulator Node の IBDA_SignalStatistics を使用する

	////////////////////////////////////////
	// BonDriver パラメータ関係
	////////////////////////////////////////

	// バッファ1個あたりのサイズ
	size_t m_nBuffSize = 1024;

	// 最大バッファ数
	size_t m_nMaxBuffCount = 512;

	// m_hOnDecodeEventをセットするデータバッファ個数
	unsigned int m_nWaitTsCount = 1;

	// WaitTsStreamで最低限待機する時間
	unsigned int m_nWaitTsSleep = 100;

	// SetChannel()でチャンネルロックに失敗した場合でもFALSEを返さないようにするかどうか
	BOOL m_bAlwaysAnswerLocked = FALSE;

	// COMProcThreadのスレッドプライオリティ
	int m_nThreadPriorityCOM = THREAD_PRIORITY_ERROR_RETURN;

	// DecodeProcThreadのスレッドプライオリティ
	int m_nThreadPriorityDecode = THREAD_PRIORITY_ERROR_RETURN;

	// ストリームスレッドプライオリティ
	int m_nThreadPriorityStream = THREAD_PRIORITY_ERROR_RETURN;

	// timeBeginPeriod()で設定するWindowsの最小タイマ分解能(msec)
	unsigned int m_nPeriodicTimer = 0;

	////////////////////////////////////////
	// チャンネルパラメータ
	////////////////////////////////////////

	// チャンネルデータ
	struct ChData {
		std::basic_string<TCHAR> sServiceName;	// EnumChannelNameで返すチャンネル名
		unsigned int Satellite = 0;			// 衛星受信設定番号
		unsigned int Polarisation = 0;		// 偏波種類番号 (0 .. 未指定, 1 .. H, 2 .. V, 3 .. L, 4 .. R)
		unsigned int ModulationType = 0;	// 変調方式設定番号
		long Frequency = 0;					// 周波数(KHz)
		union {
			long SID = -1;					// サービスID
			long PhysicalChannel;			// ATSC / Digital Cable用
		};
		union {
			long TSID = -1;					// トランスポートストリームID
			long Channel;					// ATSC / Digital Cable用
		};
		union {
			long ONID = -1;					// オリジナルネットワークID
			long MinorChannel;				// ATSC / Digital Cable用
		};
		long MajorChannel = -1;				// Digital Cable用
		long SourceID = -1;					// Digital Cable用
		BOOL LockTwiceTarget = FALSE;		// CH切替動作を強制的に2度行う対象
	};

	// チューニング空間データ
	struct TuningSpaceData {
		std::basic_string<TCHAR> sTuningSpaceName;								// EnumTuningSpaceで返すTuning Space名
		long FrequencyOffset = 0;												// 周波数オフセット値
		unsigned int DVBSystemTypeNumber = 0;									// TuningSpaceの種類番号
		EnumSettingValue::TSMFMode TSMFMode = EnumSettingValue::TSMFMode::Off;	// TSMFの処理モード
		std::map<unsigned int, ChData> Channels;								// チャンネル番号とチャンネルデータ
		DWORD dwNumChannel = 0;													// チャンネル数
	};

	// チューニングスペース一覧
	struct TuningData {
		std::map<unsigned int, TuningSpaceData> Spaces;	// チューニングスペース番号とデータ
		DWORD dwNumSpace = 0;							// チューニングスペース数
	};
	TuningData m_TuningData;

	////////////////////////////////////////
	// 衛星受信パラメータ
	////////////////////////////////////////

	// iniファイルで受付ける偏波種類数
	static constexpr unsigned int POLARISATION_SIZE = 5U;

	// CBonTunerで使用する偏波種類番号とPolarisation型のMapping
	static constexpr Polarisation PolarisationMapping[POLARISATION_SIZE] = {
		BDA_POLARISATION_NOT_SET,
		BDA_POLARISATION_LINEAR_H,
		BDA_POLARISATION_LINEAR_V,
		BDA_POLARISATION_CIRCULAR_L,
		BDA_POLARISATION_CIRCULAR_R
	};

	// 偏波種類毎のiniファイルでの記号
	static constexpr WCHAR PolarisationChar[POLARISATION_SIZE] = {
		L' ',
		L'H',
		L'V',
		L'L',
		L'R'
	};

	// iniファイルで設定できる最大衛星数 + 1
	static constexpr unsigned int MAX_SATELLITE = 10U;

	// 衛星受信設定データ
	struct Satellite {
		AntennaParam Polarisation[POLARISATION_SIZE];	// 偏波種類毎のアンテナ設定
	};
	Satellite m_aSatellite[MAX_SATELLITE];

	// チャンネル名の自動生成に使用する衛星の名称
	std::wstring m_sSatelliteName[MAX_SATELLITE];

	////////////////////////////////////////
	// 変調方式パラメータ
	////////////////////////////////////////

	// iniファイルで設定できる最大変調方式数
	static constexpr unsigned int MAX_MODULATION = 10U;

	// 変調方式設定データ
	ModulationMethod m_aModulationType[MAX_MODULATION];

	// チャンネル名の自動生成に使用する変調方式の名称
	std::wstring m_sModulationName[MAX_MODULATION];

	////////////////////////////////////////
	// BonDriver 関連
	////////////////////////////////////////

	// iniファイルのPath
	std::wstring m_sIniFilePath;

	// 受信イベント
	HANDLE m_hOnStreamEvent = NULL;

	// デコード完了イベント
	HANDLE m_hOnDecodeEvent = NULL;

	// WaitTsStreamで使用するイベントのポインタ
	HANDLE* m_phOnWaitTsEvent = NULL;

	// 受信TSデータバッファ
	TS_BUFF* m_pTsBuff = NULL;

	// Decode処理の終わったTSデータバッファ
	TS_BUFF* m_pDecodedTsBuff = NULL;

	// GetTsStreamで使用するデータバッファのポインタ
	TS_BUFF** m_ppGetTsBuff = NULL;

	// GetTsStreamで参照されるバッファ
	TS_DATA* m_LastBuff = NULL;

	// データ受信中
	BOOL m_bRecvStarted = FALSE;

	// プロセスハンドル
	HANDLE m_hProcess = NULL;

	// ストリームスレッドのハンドル
	HANDLE m_hStreamThread = NULL;

	// ストリームスレッドハンドル通知フラグ
	BOOL m_bIsSetStreamThread = FALSE;

	// ビットレート計算用
	CBitRate* m_pBitRate = NULL;
	BOOL m_bNeedBitRate = FALSE;

	// TSMF処理用
	CTSMFParser* m_pTSMFParser = NULL;
	BOOL m_bNeedTSMFParser = FALSE;

	////////////////////////////////////////
	// チューナ関連
	////////////////////////////////////////

	// チューナデバイス排他処理用
	HANDLE m_hSemaphore = NULL;

	// Graph
	CComPtr<IGraphBuilder> m_pIGraphBuilder;	// Filter Graph Manager の IGraphBuilder interface
	CComPtr<IMediaControl> m_pIMediaControl;	// Filter Graph Manager の IMediaControl interface
	CComPtr<IBaseFilter> m_pNetworkProvider;	// NetworkProvider の IBaseFilter interface
	CComPtr<ITuner> m_pITuner;					// NetworkProvider の ITuner interface
	CComPtr<ITuningSpace> m_pITuningSpace;		// NetworkProvider が使用する TuningSpace の ITuningSpace interface
	CComPtr<IBaseFilter> m_pTunerDevice;		// Tuner Device の IBaseFilter interface
	CComPtr<IBaseFilter> m_pCaptureDevice;		// Capture Device の IBaseFilter interface
	CComPtr<IBaseFilter> m_pTsWriter;			// CTsWriter の IBaseFilter interface
	CComPtr<ITsWriter> m_pITsWriter;			// CTsWriter の ITsWriter interface
	CComPtr<IBaseFilter> m_pDemux;				// MPEG2 Demultiplexer の IBaseFilter interface
	CComPtr<IBaseFilter> m_pTif;				// MPEG2 Transport Information Filter の IBaseFilter interface

	// チューナ信号状態取得用インターフェース
	CComPtr<IBDA_SignalStatistics> m_pIBDA_SignalStatisticsTunerNode;
	CComPtr<IBDA_SignalStatistics> m_pIBDA_SignalStatisticsDemodNode;

	// チューナーの使用するTuningSpaceの種類データ
	struct DVBSystemTypeData {
		EnumSettingValue::TunerType nDVBSystemType = EnumSettingValue::TunerType::None;								// チューナーの使用するTuningSpaceの種類
		EnumSettingValue::TuningSpace nTuningSpace = EnumSettingValue::TuningSpace::Auto;							// 使用するTuningSpace オブジェクト
		EnumSettingValue::Locator nLocator = EnumSettingValue::Locator::Auto;										// 使用するLocator オブジェクト
		EnumSettingValue::NetworkType nITuningSpaceNetworkType = EnumSettingValue::NetworkType::Auto;				// ITuningSpaceに設定するNetworkType
		EnumSettingValue::DVBSystemType nIDVBTuningSpaceSystemType = EnumSettingValue::DVBSystemType::Auto;			// IDVBTuningSpaceに設定するSystemType
		EnumSettingValue::TunerInputType nIAnalogTVTuningSpaceInputType = EnumSettingValue::TunerInputType::Auto;	// IAnalogTVTuningSpaceに設定するInputType
		EnumSettingValue::NetworkProvider nNetworkProvider = EnumSettingValue::NetworkProvider::Auto;				// チューナーに使用するNetworkProvider
		unsigned int nTunerGroup = 0;																				// 使用するTunerGroup番号
	};

	// TuningSpaceの種類データベース
	struct DVBSystemTypeDB {
		std::map<unsigned int, DVBSystemTypeData> SystemType;	// TuningSpaceの種類番号とTuningSpaceの種類データ
		unsigned int nNumType = 0;								// TuningSpaceの種類数
		BOOL IsExist(unsigned int number)
		{
			auto it = SystemType.find(number);
			if (it == SystemType.end())
				return FALSE;
			return TRUE;
		}
	};
	DVBSystemTypeDB m_DVBSystemTypeDB;

	// 変更を試みるTuningSpaceの種類番号
	long m_nTargetDvbSystemTypeNum = 0;

	// カレントTuningSpaceの種類番号
	long m_nCurrentDvbSystemTypeNum = -1;

	// iniファイルで定義できる最大TuningSpaceの種類データ数
	static constexpr unsigned int MAX_DVB_SYSTEM_TYPE = 10U;

	// 衛星受信パラメータ/変調方式パラメータのデフォルト値
	EnumSettingValue::DefaultNetwork m_nDefaultNetwork = EnumSettingValue::DefaultNetwork::SPHD;

	// Tuner is opened
	BOOL m_bOpened = FALSE;

	// チューニングスペース番号不明
	static constexpr DWORD SPACE_INVALID = 0xFFFFFFFFUL;

	// SetChannel()を試みたチューニングスペース番号
	DWORD m_dwTargetSpace = SPACE_INVALID;

	// カレントチューニングスペース番号
	DWORD m_dwCurSpace = SPACE_INVALID;

	// チャンネル番号不明
	static constexpr DWORD CHANNEL_INVALID = 0xFFFFFFFFUL;

	// SetChannel()を試みたチャンネル番号
	DWORD m_dwTargetChannel = CHANNEL_INVALID;

	// カレントチャンネル番号
	DWORD m_dwCurChannel = CHANNEL_INVALID;

	// トーン切替状態不明
	static constexpr long TONE_UNKNOWN = -1L;

	// 現在のトーン切替状態
	long m_nCurTone = TONE_UNKNOWN;

	// 最後にLockChannelを行った時のチューニングパラメータ
	TuningParam m_LastTuningParam;

	// TunerSpecial DLL module handle
	HMODULE m_hModuleTunerSpecials = NULL;

	// チューナ固有関数 IBdaSpecials
	IBdaSpecials* m_pIBdaSpecials = NULL;
	IBdaSpecials2b5* m_pIBdaSpecials2 = NULL;

	// チューナ固有の関数が必要かどうかを自動判別するDB
	// GUID をキーに DLL 名を得る
	struct TUNER_SPECIAL_DLL {
		const WCHAR * const sTunerGUID;
		const WCHAR * const sDLLBaseName;
	};
	static constexpr TUNER_SPECIAL_DLL aTunerSpecialData[] = {
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

	// チャンネル名自動生成 inline 関数
	inline std::basic_string<TCHAR> MakeChannelName(const CBonTuner::ChData* const pChData)
	{
		std::basic_string<TCHAR> format;
		long m = pChData->Frequency / 1000;
		long k = pChData->Frequency % 1000;
		if (k == 0)
			format = _T("%s/%05ld%c/%s");
		else
			format = _T("%s/%05ld.%03ld%c/%s");
		return common::TStringPrintf(format.c_str(), m_sSatelliteName[pChData->Satellite].c_str(), m, PolarisationChar[pChData->Polarisation], m_sModulationName[pChData->ModulationType].c_str());
	}
};

