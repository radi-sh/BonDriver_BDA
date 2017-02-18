#pragma once

#include <Windows.h>
#include <stdio.h>

#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }

static FILE *g_fpLog = NULL;

static inline void SetDebugLog(WCHAR *szLogPath)
{
	if (g_fpLog) {
		// 別のインスタンスにより開かれている
		return;
	}

	g_fpLog = _wfsopen(szLogPath, L"a+", _SH_DENYNO);

	return;
}

static inline void CloseDebugLog(void)
{
	if (g_fpLog) {
		fclose(g_fpLog);
		g_fpLog = NULL;
	}

	return;
}

static inline void OutputDebug(LPCWSTR format, ...)
{
	WCHAR buffer[2048];
	va_list ap;
	va_start(ap, format);
	vswprintf_s(buffer, sizeof(buffer) / sizeof(buffer[0]), format, ap);
	va_end(ap);
	::OutputDebugStringW(buffer);

	if (g_fpLog) {
		fwprintf(g_fpLog, buffer);
		fflush(g_fpLog);
	}
}
