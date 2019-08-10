#include "common.h"

#include <Windows.h>
#include <string>
#include <tchar.h>
#include <algorithm>

#pragma comment(lib, "Rpcrt4.lib")

std::string common::StringPrintf(LPCSTR format, ...)
{
	char buffer_c[2048];
	va_list ap;
	va_start(ap, format);
	::vsnprintf_s(buffer_c, sizeof(buffer_c) / sizeof(buffer_c[0]), format, ap);
	va_end(ap);

	return std::string(buffer_c);
}

std::wstring common::WStringPrintf(LPCWSTR format, ...)
{
	WCHAR buffer[2048];
	va_list ap;
	va_start(ap, format);
	::vswprintf_s(buffer, sizeof(buffer) / sizeof(buffer[0]), format, ap);
	va_end(ap);

	return std::wstring(buffer);
}

std::basic_string<TCHAR> common::TStringPrintf(LPCTSTR format, ...)
{
	std::basic_string<TCHAR> str;
	TCHAR buffer_t[2048];
	va_list ap;
	va_start(ap, format);
	::_vstprintf_s(buffer_t, sizeof(buffer_t) / sizeof(buffer_t[0]), format, ap);
	va_end(ap);

	return std::basic_string<TCHAR>(buffer_t);
}

std::string common::WStringToString(std::wstring Src)
{
#pragma warning (push)
#pragma warning (disable: 4244)
	return std::string(Src.cbegin(), Src.cend());
#pragma warning (pop)
}

std::wstring common::StringToWString(std::string Src)
{
	return std::wstring(Src.cbegin(), Src.cend());
}

std::basic_string<TCHAR> common::WStringToTString(std::wstring Src)
{
#pragma warning (push)
#pragma warning (disable: 4244)
	return std::basic_string<TCHAR>(Src.cbegin(), Src.cend());
#pragma warning (pop)
}

std::wstring common::WStringToUpperCase(std::wstring Src)
{
	std::transform(Src.cbegin(), Src.cend(), Src.begin(), ::towupper);
	return Src;
}

std::wstring common::WStringToLowerCase(std::wstring Src)
{
	std::transform(Src.cbegin(), Src.cend(), Src.begin(), ::towlower);
	return Src;
}

int common::WStringToLong(std::wstring Src)
{
	return ::wcstol(Src.c_str(), NULL, 0);
}

int common::WStringDecimalToLong(std::wstring Src)
{
	return ::wcstol(Src.c_str(), NULL, 10);
}

double common::WstringToDouble(std::wstring Src)
{
	return ::wcstod(Src.c_str(), NULL);
}

std::wstring common::GUIDToWString(GUID guid)
{
	RPC_STATUS rpcret;
	WCHAR *wszGuid = NULL;
	std::wstring str;
	if ((rpcret = ::UuidToStringW(&guid, (RPC_WSTR *)&wszGuid)) == RPC_S_OK) {
		str = wszGuid;
		::RpcStringFreeW((RPC_WSTR *)&wszGuid);
	}
	return  str;
}

std::wstring common::GetModuleName(HMODULE hModule)
{
	WCHAR buffer[_MAX_PATH + 1];
	::GetModuleFileNameW(hModule, buffer, sizeof(buffer) / sizeof(buffer[0]));
	std::wstring tempPath(buffer);
	return tempPath.substr(0, tempPath.length() - 3);
}

std::wstring::size_type common::WStringSplit(std::wstring * Src, WCHAR Separator, std::wstring * Out)
{
	std::wstring::size_type pos = Src->find(Separator);
	if (pos == std::wstring::npos) {
		*Out = *Src;
		*Src = L"";
	}
	else {
		*Out = Src->substr(0, pos);
		*Src = Src->substr(pos + 1, Src->length() - pos - 1);
	}
	return pos;
}