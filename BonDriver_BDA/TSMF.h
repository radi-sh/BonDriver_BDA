#pragma once

#include <Windows.h>

class CTSMFParser
{
private:
	int slot_counter;										// TSMF多重フレームスロット番号
	WORD TSID;												// 抽出するストリームのTSIDまたは相対TS番号
	WORD ONID;												// 抽出するストリームのONID
	BOOL IsRelative;										// 相対TS番号で指定するかどうか FALSE..ONID/TSIDで指定, TRUE..相対TS番号で指定
	size_t PacketSize;										// TSパケットサイズ
	BYTE * prevBuf;											// 前回処理したTSパケットバッファ(未処理半端分保存用)
	size_t prevBufSize;										// 前回処理したTSパケットバッファのサイズ
	size_t prevBufPos;										// 前回処理したTSパケットバッファの処理開始位置
	struct {
		BYTE continuity_counter;							// 連続性指標
		BYTE version_number;								// 変更指示
		BYTE relative_stream_number_mode;					// スロット配置法の区別
		BYTE frame_type;									// 多重フレーム形式
		struct {
			BYTE stream_status;								// 相対ストリーム番号に対する有効、無効指示
			WORD stream_id;									// ストリーム識別／相対ストリーム番号対応情報
			WORD original_network_id;						// オリジナルネットワ－ク識別／相対ストリーム番号対応情報
			BYTE receive_status;							// 受信状態
		} stream_info[15];									// 相対ストリーム番号毎の情報
		BYTE emergency_indicator;							// 緊急警報指示
		BYTE relative_stream_number[52];					// 相対ストリーム番号対スロット対応情報
	} TSMFData;												// TSMF多重フレームヘッダ情報
	static constexpr BYTE TS_PACKET_SYNC_BYTE = 0x47;		// TSパケットヘッダ同期バイトコード

public:
	// コンストラクタ
	CTSMFParser(void);
	// デストラクタ
	~CTSMFParser(void);
	// ストリーム識別子をセット
	void SetTSID(WORD onid, WORD tsid, BOOL relative);
	// TSMF処理を無効にする
	void Disable(void);
	// TSバッファのTSMF処理を行う
	void ParseTsBuffer(BYTE * buf, size_t len, BYTE ** newBuf, size_t * newBufLen);

private:
	// 全ての情報をクリア
	void Clear(void);
	// TSMFヘッダの解析を行う
	BOOL ParseTSMFHeader(const BYTE * buf, size_t len);
	// 1パケット(1フレーム)の処理を行う
	BOOL ParseOnePacket(const BYTE * buf, size_t len);
	// TSパケットの同期を行う
	BOOL SyncPacket(const BYTE * buf, size_t len, size_t * truncate, size_t * packetSize);
};