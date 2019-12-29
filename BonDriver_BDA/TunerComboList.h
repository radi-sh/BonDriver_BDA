#pragma once

#include <Windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>

struct IBaseFilter;
class CDSFilterEnum;

class CTunerComboList
{
public:
	// コンストラクタ
	CTunerComboList(void);

	// 検索データのクリア
	void ClearSearchData(void);

	// 検索データの追加
	void AddSearchData(unsigned int tunerGroup, unsigned int order, std::wstring tunerGuid, std::wstring tunerFriendlyName, std::wstring captureGuid, std::wstring captureFriendlyName);

	// 検索データからTuner / Captureの組合せDataBaseを作成
	void BuildComboDB(void);

	// Tunerリストの先頭へ移動
	BOOL ResetTuner(unsigned int group);

	// Tunerリストの次の対象へ移動
	BOOL NextTuner(void);

	// Tunerのデータを取得
	void GetTunerData(const std::wstring** guid, const std::wstring** friendlyName, const ULONG** order);

	// Tunerデバイスフィルタを取得
	HRESULT GetTunerFilter(IBaseFilter** ppFilter);

	// Captureリストの先頭へ移動
	BOOL ResetCapture(void);

	// Captureリストの次の対象へ移動
	BOOL NextCapture(void);

	// Captureのデータを取得
	void GetCaptureData(const std::wstring** guid, const std::wstring** friendlyName, const ULONG** order);

	// Captureデバイスフィルタを取得
	HRESULT GetCaptureFilter(IBaseFilter** ppFilter);

	// 使用したTunerを記憶
	void Remenber(void);

	// 記憶したTunerをクリア
	void Forget();

	// 記憶したTunerをListの最後尾に移動する
	void PutOff();

private:
	static void ReassembleDeviceInstancePath(std::wstring* deviceInstancePath);

public:
	// TunerデバイスのみでCaptureデバイスが存在しない場合TRUE
	BOOL bNotExistCaptureDevice;

	// TunerとCaptureのデバイスインスタンスパスが一致しているかの確認を行うかどうか
	BOOL bCheckDeviceInstancePath;

private:
	// チューナ・キャプチャ検索に使用するGUID文字列とFriendlyName文字列の組合せ
	struct TunerSearchData {
		std::wstring TunerGUID;					// TunerデバイスのDisplay Name
		std::wstring TunerFriendlyName;			// TunerデバイスのFrriendly Name
		std::wstring CaptureGUID;				// CaptureデバイスのDisplay Name
		std::wstring CaptureFriendlyName;		// CaptureデバイスのFrriendly Name
		// デフォルトコンストラクタ
		TunerSearchData(void);
		// コンストラクタ
		TunerSearchData(std::wstring tunerGuid, std::wstring tunerFriendlyName, std::wstring captureGuid, std::wstring captureFriendlyName);
	};

	// TunerGroup 1個分のINIファイルで指定された Guid / FriendlyName / CaptureGuid / CaptureFriendlyName のList
	typedef std::map<unsigned int, TunerSearchData> TunerMap;

	// TunerGroup 10個分の Guid / FriendlyName / CaptureGuid / CaptureFriendlyName の DataBase
	std::map<unsigned int, TunerMap> TunerSearchDB;

	// DSフィルター列挙 CDSFilterEnum
	CDSFilterEnum* pDSFilterEnumTuner;
	CDSFilterEnum* pDSFilterEnumCapture;

	// Tuner / Capture 単体のDSフィルター情報
	struct DSListData {
		std::wstring GUID;						// Display Name
		std::wstring FriendlyName;				// Friendly Name
		ULONG Order;							// CDSFilterEnum列挙のOrder番号
		// コンストラクタ
		DSListData(std::wstring _GUID, std::wstring _FriendlyName, ULONG _Order)
			: GUID(_GUID),
			FriendlyName(_FriendlyName),
			Order(_Order)
		{
		}
	};

	// Tuner単体とそれに紐づく複数のCaptureの組合せ
	struct TunerCaptureListData {
		DSListData Tuner;						// Tunerの情報
		std::vector<DSListData> CaptureList;	// Tunerと紐づける複数のCaptureの情報
		// コンストラクタ
		TunerCaptureListData(std::wstring TunerGUID, std::wstring TunerFriendlyName, ULONG TunerOrder)
			: Tuner(TunerGUID, TunerFriendlyName, TunerOrder)
		{
		}
		// コンストラクタ
		TunerCaptureListData(DSListData _Tuner)
			: Tuner(_Tuner)
		{
		}
	};

	// TunerGroup 1個分の Tuner情報 / Capture情報の組合せList
	typedef std::list<TunerCaptureListData> TunerCaptureList;

	// TunerGroup 10個分の Tuner情報 / Capture情報 DataBase
	std::map<unsigned int, TunerCaptureList> TunerComboDB;

	// Tuner情報 / Capture情報 DataBaseのイテレータ
	TunerCaptureList::iterator itTuner;

	// Captur情報のイテレータ
	std::vector<DSListData>::iterator itCapture;

	// カレントTunerGroup番号
	unsigned int CurrentGroup;

	// Connect成功した組合せ記憶用
	TunerCaptureList::iterator LastConnecttedTuner;
	
	// Connect成功したTunerGroup番号記憶用
	unsigned int LastConnecttedGroup;

	// Connect成功した記憶が無い時のLastConnecttedGroup値
	static constexpr unsigned int NO_MEMORY = (unsigned int)-1;
};
