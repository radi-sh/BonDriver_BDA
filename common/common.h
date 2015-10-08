#pragma once

#include <Windows.h>
#include <stdio.h>

#ifdef RtlMoveMemory
#undef RtlMoveMemory
#endif
EXTERN_C NTSYSAPI VOID NTAPI
RtlMoveMemory (LPVOID UNALIGNED Dst, LPCVOID UNALIGNED Src, SIZE_T Length);

#ifdef RtlFillMemory
#undef RtlFillMemory
#endif
EXTERN_C NTSYSAPI VOID NTAPI
RtlFillMemory (LPVOID UNALIGNED Dst, SIZE_T Length, BYTE Pattern);

#ifdef RtlZeroMemory
#undef RtlZeroMemory
#endif
EXTERN_C NTSYSAPI VOID NTAPI
RtlZeroMemory (LPVOID UNALIGNED Dst, SIZE_T Length);

#ifdef CopyMemory
#undef CopyMemory
#endif
#define CopyMemory(Destination,Source,Length) RtlMoveMemory((Destination),(Source),(Length))

#ifdef MoveMemory
#undef MoveMemory
#endif
#define MoveMemory(Destination,Source,Length) RtlMoveMemory((Destination),(Source),(Length))

#ifdef FillMemory
#undef FillMemory
#endif
#define FillMemory(Destination,Length,Fill) RtlFillMemory((Destination),(Length),(Fill))

#ifdef ZeroMemory
#undef ZeroMemory
#endif
#define ZeroMemory(Destination,Length) RtlZeroMemory((Destination),(Length))

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

	_wfopen_s(&g_fpLog, szLogPath, L"a+");

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
	}
	fflush(g_fpLog);
}
