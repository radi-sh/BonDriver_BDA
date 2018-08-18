#include "common.h"

#include <Windows.h>
#include <string>
#include <tchar.h>
#include <algorithm>

namespace common {

	std::string StringPrintf(LPCSTR format, ...)
	{
		char buffer_c[2048];
		va_list ap;
		va_start(ap, format);
		::vsnprintf_s(buffer_c, sizeof(buffer_c) / sizeof(buffer_c[0]), format, ap);
		va_end(ap);

		return std::string(buffer_c);
	}

	std::wstring WStringPrintf(LPCWSTR format, ...)
	{
		WCHAR buffer[2048];
		va_list ap;
		va_start(ap, format);
		::vswprintf_s(buffer, sizeof(buffer) / sizeof(buffer[0]), format, ap);
		va_end(ap);

		return std::wstring(buffer);
	}

	std::basic_string<TCHAR> TStringPrintf(LPCTSTR format, ...)
	{
		std::basic_string<TCHAR> str;
		TCHAR buffer_t[2048];
		va_list ap;
		va_start(ap, format);
		::_vstprintf_s(buffer_t, sizeof(buffer_t) / sizeof(buffer_t[0]), format, ap);
		va_end(ap);

		return std::basic_string<TCHAR>(buffer_t);
	}

	std::string WStringToString(std::wstring Src)
	{
		char buffer_c[2048];
		::wcstombs_s(NULL, buffer_c, Src.c_str(), sizeof(buffer_c) / sizeof(buffer_c[0]));

		return std::string(buffer_c);
	}

	std::basic_string<TCHAR> WStringToTString(std::wstring Src)
	{
#ifdef UNICODE
		return Src;
#else
		char buffer_c[2048];
		::wcstombs_s(NULL, buffer_c, Src.c_str(), sizeof(buffer_c) / sizeof(buffer_c[0]));

		return std::string(buffer_c);
#endif
	}

	std::wstring WStringToUpperCase(std::wstring Src)
	{
		std::transform(Src.cbegin(), Src.cend(), Src.begin(), ::towupper);
		return Src;
	}

	std::wstring WStringToLowerCase(std::wstring Src)
	{
		std::transform(Src.cbegin(), Src.cend(), Src.begin(), ::towlower);
		return Src;
	}

	int WStringToLong(std::wstring Src)
	{
		return ::wcstol(Src.c_str(), NULL, 0);
	}

	int WStringDecimalToLong(std::wstring Src)
	{
		return ::wcstol(Src.c_str(), NULL, 10);
	}

	double WstringToDouble(std::wstring Src)
	{
		return ::wcstod(Src.c_str(), NULL);
	}

	std::wstring GetModuleName(HMODULE hModule)
	{
		WCHAR buffer[_MAX_PATH + 1];
		::GetModuleFileNameW(hModule, buffer, sizeof(buffer) / sizeof(buffer[0]));
		std::wstring tempPath(buffer);
		return tempPath.substr(0, tempPath.length() - 3);
	}
}