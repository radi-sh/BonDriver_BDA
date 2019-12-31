#pragma once

#include <Windows.h>
#include <queue>

// TSデータバッファ
class TS_DATA {
public:
	BYTE* pbyBuff = NULL;
	size_t Size = 0;
public:
	TS_DATA(BYTE* data, size_t size, BOOL copy = FALSE);
	~TS_DATA(void);
};

// TSデータバッファ列
class TS_BUFF {
private:
	std::queue<TS_DATA *> List;
	BYTE* TempBuff = NULL;
	size_t TempOffset = 0;
	size_t BuffSize = 0;
	size_t MaxCount = 0;
	CRITICAL_SECTION cs = {};

public:
	TS_BUFF(void);
	TS_BUFF(size_t buffSize, size_t maxCount);
	~TS_BUFF(void);
	void SetSize(size_t buffSize, size_t maxCount);
	void Purge(void);
	void Add(TS_DATA *pItem);
	BOOL AddData(BYTE *pbyData, size_t size);
	TS_DATA* Get(void);
	size_t Size(void);
};

