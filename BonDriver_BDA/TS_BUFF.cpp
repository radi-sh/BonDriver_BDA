#include "common.h"

#include "TS_BUFF.h"

TS_DATA::TS_DATA(void)
	: pbyBuff(NULL),
	Size(0)
{
}

TS_DATA::TS_DATA(BYTE* data, size_t size, BOOL copy)
{
	if (copy) {
		pbyBuff = new BYTE[size];
		memcpy(pbyBuff, data, size);
	}
	else {
		pbyBuff = data;
	}
	Size = size;
}

TS_DATA::~TS_DATA(void) {
	SAFE_DELETE_ARRAY(pbyBuff);
}

TS_BUFF::TS_BUFF(void)
	: TempBuff(NULL),
	TempOffset(0),
	BuffSize(0),
	MaxCount(0)
{
	::InitializeCriticalSection(&cs);
}

TS_BUFF::~TS_BUFF(void)
{
	Purge();
	SAFE_DELETE_ARRAY(TempBuff);
	::DeleteCriticalSection(&cs);
}

void TS_BUFF::SetSize(size_t buffSize, size_t maxCount)
{
	Purge();
	SAFE_DELETE_ARRAY(TempBuff);
	if (buffSize) {
		TempBuff = new BYTE[buffSize];
	}
	BuffSize = buffSize;
	MaxCount = maxCount;
}

void TS_BUFF::Purge(void)
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

void TS_BUFF::Add(TS_DATA *pItem)
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

BOOL TS_BUFF::AddData(BYTE *pbyData, size_t size)
{
	BOOL ret = false;
	while (size) {
		TS_DATA *pItem = NULL;
		::EnterCriticalSection(&cs);
		if (TempBuff) {
			// iniファイルでBuffSizeが指定されている場合はそのサイズに合わせる
			size_t copySize = (BuffSize > TempOffset + size) ? size : BuffSize - TempOffset;
			memcpy(TempBuff + TempOffset, pbyData, copySize);
			TempOffset += copySize;
			size -= copySize;
			pbyData += copySize;

			if (TempOffset >= BuffSize) {
				// テンポラリバッファのデータを追加
				pItem = new TS_DATA(TempBuff, TempOffset, FALSE);
				TempBuff = new BYTE[BuffSize];
				TempOffset = 0;
			}
		}
		else {
			// BuffSizeが指定されていない場合は上流から受け取ったサイズでそのまま追加
			pItem = new TS_DATA(pbyData, size, TRUE);
			size = 0;
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

TS_DATA * TS_BUFF::Get(void)
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

size_t TS_BUFF::Size(void)
{
	return List.size();
}
