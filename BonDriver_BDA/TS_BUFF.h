#pragma once

#include <Windows.h>
#include <queue>

// TSデータバッファ
struct TS_DATA {
	BYTE* pbyBuff;
	size_t Size;
	TS_DATA(void);
	TS_DATA(BYTE* data, size_t size, BOOL copy = FALSE);
	~TS_DATA(void);
};

// TSデータバッファ列
class TS_BUFF {
private:
	std::queue<TS_DATA *> List;
	BYTE *TempBuff;
	size_t TempOffset;
	size_t BuffSize;
	size_t MaxCount;
	CRITICAL_SECTION cs;

public:
	TS_BUFF(void);
	~TS_BUFF(void);
	void SetSize(size_t buffSize, size_t maxCount);
	void Purge(void);
	void Add(TS_DATA *pItem);
	BOOL AddData(BYTE *pbyData, size_t size);
	TS_DATA * Get(void);
	size_t Size(void);
};

