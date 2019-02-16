#include "common.h"

#include "CIniFileAccess.h"

static const std::map<const std::wstring, const int, std::less<>> mapBool = {
	{ L"NO",    0 },
	{ L"YES",   1 },
	{ L"FALSE", 0 },
	{ L"TRUE",  1 },
	{ L"OFF",   0 },
	{ L"ON",    1 },
};

CIniFileAccess::CIniFileAccess()
{
	CIniFileAccess(L"");
}

CIniFileAccess::CIniFileAccess(const std::wstring IniFilePath)
	: m_KeysSize(0)
{
	SetIniFilePath(IniFilePath);
	m_SectionDataIterator = m_SectionData.begin();
}

CIniFileAccess::~CIniFileAccess()
{
	m_SectionData.clear();
}

void CIniFileAccess::SetIniFilePath(const std::wstring IniFilePath)
{
	m_IniFilePath = IniFilePath;
}

void CIniFileAccess::SetSectionName(const std::wstring SectionName)
{
	m_SectionName = SectionName;
}

int CIniFileAccess::ReadKeyI(const std::wstring KeyName, int default)
{
	return ::GetPrivateProfileIntW(m_SectionName.c_str(), KeyName.c_str(), default, m_IniFilePath.c_str());
}

int CIniFileAccess::ReadKeyI(const std::wstring SectionName, const std::wstring KeyName, int default)
{
	return ::GetPrivateProfileIntW(SectionName.c_str(), KeyName.c_str(), default, m_IniFilePath.c_str());
}

BOOL CIniFileAccess::ReadKeyB(const std::wstring KeyName, BOOL default)
{
	return ReadKeyB(m_SectionName, KeyName, default);
}

BOOL CIniFileAccess::ReadKeyB(const std::wstring SectionName, const std::wstring KeyName, BOOL default)
{
	return ReadKeyIValueMap(SectionName, KeyName, default, mapBool);
}

int CIniFileAccess::ReadKeyIValueMap(const std::wstring KeyName, int default, const std::map<const std::wstring, const int, std::less<>> ValueMap)
{
	return CIniFileAccess::ReadKeyIValueMap(m_SectionName, KeyName, default, ValueMap);
}

int CIniFileAccess::ReadKeyIValueMap(const std::wstring SectionName, const std::wstring KeyName, int default, const std::map<const std::wstring, const int, std::less<>> ValueMap)
{
	WCHAR buf[_MAX_PATH + 1];
	::GetPrivateProfileStringW(SectionName.c_str(), KeyName.c_str(), L"", buf, sizeof(buf) / sizeof(buf[0]), m_IniFilePath.c_str());
	auto it = ValueMap.find(common::WStringToUpperCase(buf));
	if (it != ValueMap.end())
		return it->second;

	return ::GetPrivateProfileIntW(SectionName.c_str(), KeyName.c_str(), default, m_IniFilePath.c_str());
}

double CIniFileAccess::ReadKeyF(const std::wstring KeyName, double default)
{
	WCHAR buf[_MAX_PATH + 1];
	::GetPrivateProfileStringW(m_SectionName.c_str(), KeyName.c_str(), std::to_wstring(default).c_str(), buf, sizeof(buf) / sizeof(buf[0]), m_IniFilePath.c_str());
	return common::WstringToDouble(buf);
}

double CIniFileAccess::ReadKeyF(const std::wstring SectionName, const std::wstring KeyName, double default)
{
	WCHAR buf[_MAX_PATH + 1];
	::GetPrivateProfileStringW(SectionName.c_str(), KeyName.c_str(), std::to_wstring(default).c_str(), buf, sizeof(buf) / sizeof(buf[0]), m_IniFilePath.c_str());
	return common::WstringToDouble(buf);
}

std::wstring CIniFileAccess::ReadKeyS(const std::wstring KeyName, std::wstring default)
{
	WCHAR buf[_MAX_PATH + 1];
	::GetPrivateProfileStringW(m_SectionName.c_str(), KeyName.c_str(), default.c_str(), buf, sizeof(buf) / sizeof(buf[0]), m_IniFilePath.c_str());
	return std::wstring(buf);
}

std::wstring CIniFileAccess::ReadKeyS(const std::wstring SectionName, const std::wstring KeyName, std::wstring default)
{
	WCHAR buf[_MAX_PATH + 1];
	::GetPrivateProfileStringW(SectionName.c_str(), KeyName.c_str(), default.c_str(), buf, sizeof(buf) / sizeof(buf[0]), m_IniFilePath.c_str());
	return std::wstring(buf);
}

int CIniFileAccess::ReadSection(const std::wstring SectionName)
{
	m_KeysSectionName = SectionName;
	m_KeysSize = (size_t)::GetPrivateProfileSectionW(SectionName.c_str(), m_KeysBuffer, sizeof(m_KeysBuffer) / sizeof(m_KeysBuffer[0]), m_IniFilePath.c_str());
	return (int)m_KeysSize;
}

int CIniFileAccess::CreateSectionData(void)
{
	m_SectionData.clear();

	WCHAR *pkey = m_KeysBuffer;
	while (pkey < m_KeysBuffer + m_KeysSize) {
		if (pkey[0] == L'\0') {
			// キー名の一覧終わり
			break;
		}
		std::wstring line = pkey;
		// 行先頭が";"ならコメント
		if (line.find(L";") == 0) {
			line = L"";
		}
		// "="の前後で分割
		size_t pos = line.find(L"=");
		if (pos != std::wstring::npos) {
			std::wstring key = line.substr(0, pos);
			std::wstring data = line.substr(pos + 1);
			size_t l, r;
			// キーの前後空白を除去
			l = key.find_first_not_of(L" \t\v\r\n");
			if (l == std::wstring::npos)
				// キーが空
				continue;
			r = key.find_last_not_of(L" \t\v\r\n");
			key = key.substr(l, r - l + 1);
			// 値の前後空白を除去
			l = data.find_first_not_of(L" \t\v\r\n");
			if (l == std::wstring::npos)
				data = L"";
			else {
				r = data.find_last_not_of(L" \t\v\r\n");
				data = data.substr(l, r - l + 1);
			}
			// 値がダブルクォーテーションで囲まれている場合は除去
			l = data.find(L"\"");
			if (l == 0) {
				r = data.rfind(L"\"");
				if (r != std::wstring::npos) {
					data = data.substr(1, r - 1);
				}
			} else {
				// 値がシングルクォーテーションで囲まれている場合は除去
				l = data.find(L"'");
				if (l == 0) {
					r = data.rfind(L"'");
					if (r != std::wstring::npos) {
						data = data.substr(1, r - 1);
					}
				}
			}
			m_SectionData.emplace(common::WStringToUpperCase(key), data);
		}
		pkey += ::wcslen(pkey) + 1;
	}
	m_SectionDataIterator = m_SectionData.begin();

	return (int)m_SectionData.size();
}

void CIniFileAccess::DeleteSectionData(void)
{
	m_SectionData.clear();
	m_SectionDataIterator = m_SectionData.begin();
}

void CIniFileAccess::Reset(void)
{
	m_SectionDataIterator = m_SectionData.begin();
}

bool CIniFileAccess::ReadSectionData(std::wstring *Key, std::wstring *Data)
{
	if (m_SectionDataIterator != m_SectionData.end()) {
		*Key = m_SectionDataIterator->first;
		*Data = m_SectionDataIterator->second;
		m_SectionDataIterator++;
		return true;
	}

	return false;
}

int CIniFileAccess::ReadKeyISectionData(const std::wstring KeyName, int default)
{
	std::wstring s = ReadKeySSectionData(KeyName, L"");
	if (s.length() == 0)
		return default;
	return common::WStringToLong(s);
}

BOOL CIniFileAccess::ReadKeyBSectionData(const std::wstring KeyName, BOOL default)
{
	return ReadKeyIValueMapSectionData(KeyName, default, mapBool);
}

int CIniFileAccess::ReadKeyIValueMapSectionData(const std::wstring KeyName, int default, const std::map<const std::wstring, const int, std::less<>> ValueMap)
{
	std::wstring s = ReadKeySSectionData(KeyName, L"");
	if (s.length() == 0)
		return default;

	auto it = ValueMap.find(common::WStringToUpperCase(s));
	if (it != ValueMap.end())
		return it->second;

	return common::WStringToLong(s);
}

double CIniFileAccess::ReadKeyFSectionData(const std::wstring KeyName, double default)
{
	std::wstring s = ReadKeySSectionData(KeyName, L"");
	if (s.length() == 0)
		return default;
	return common::WstringToDouble(s);
}

std::wstring CIniFileAccess::ReadKeySSectionData(const std::wstring KeyName, std::wstring default)
{
	auto it = m_SectionData.find(common::WStringToUpperCase(KeyName));
	if (it == m_SectionData.end()) {
		return default;
	}
	return it->second;
}

