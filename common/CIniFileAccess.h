#pragma once

#include "common.h"

#include <Windows.h>
#include <string>
#include <map>

class CIniFileAccess
{
public:
	CIniFileAccess();
	CIniFileAccess(const std::wstring IniFilePath);
	~CIniFileAccess();
	void SetIniFilePath(const std::wstring IniFilePath);
	void SetSectionName(const std::wstring SectionName);
	int ReadKeyI(const std::wstring KeyName, int default);
	int ReadKeyI(const std::wstring SectionName, const std::wstring KeyName, int default);
	double ReadKeyF(const std::wstring KeyName, double default);
	double ReadKeyF(const std::wstring SectionName, const std::wstring KeyName, double default);
	std::wstring ReadKeyS(const std::wstring KeyName, std::wstring default);
	std::wstring ReadKeyS(const std::wstring SectionName, const std::wstring KeyName, std::wstring default);
	int ReadSection(const std::wstring SectionName);
	int CreateSectionData(void);
	void DeleteSectionData(void);
	void Reset(void);
	bool ReadSectionData(std::wstring *Key, std::wstring *Data);
	int ReadKeyISectionData(const std::wstring KeyName, int default);
	double ReadKeyFSectionData(const std::wstring KeyName, double default);
	std::wstring ReadKeySSectionData(const std::wstring KeyName, std::wstring default);

private:
	std::wstring m_IniFilePath;
	std::wstring m_SectionName;
	WCHAR m_KeysBuffer[(_MAX_PATH + 1) * 100];
	size_t m_KeysSize;
	std::wstring m_KeysSectionName;
	WCHAR * m_Pointer;
	std::multimap<std::wstring, std::wstring> m_SectionData;
	std::multimap<std::wstring, std::wstring>::iterator m_SectionDataIterator;
};

