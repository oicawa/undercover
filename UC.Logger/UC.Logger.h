#pragma once

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>

#define UC_LOGGER_INI_FILE_PATH ".\\UC.Logger.ini"
//#define DEBUG_LOG_INV_FILTER _T("DEBUG_LOG_INV")

typedef void(*Logger)(LPCTSTR filter, LPCTSTR message);

typedef bool (*ProcessIniKey)(TCHAR* pIniKey, void* pContext);

// --------------------------------------------------
// Public function declarations
// --------------------------------------------------
#ifdef __cplusplus
extern "C"
{
#endif
	__declspec(dllexport) void UCLogger_ManageLogger(Logger logger, bool add);
	__declspec(dllexport) bool UCLogger_IsTarget(LPCTSTR filter);
	__declspec(dllexport) void UCLogger_MakeLog(LPCTSTR filter, LPCTSTR file, int line, LPCTSTR function, LPTSTR log, size_t countOfLog, LPCTSTR format, ...);
	__declspec(dllexport) void UCLogger_DebugLog(LPCTSTR filter, LPCTSTR log);
	__declspec(dllexport) void UCLogger_DebugLogStack(LPCTSTR filter, LPCTSTR filterEx, LPCTSTR file, int line, LPCTSTR function);
	__declspec(dllexport) bool UCLogger_MakeBitsString(unsigned long long unsignedValue, size_t byteSizeOfValue, LPTSTR pBuffer, size_t countOfBuffer);
	__declspec(dllexport) bool UCLogger_WatchIniFileLastUpdated(DWORD& lastTickCount, time_t& lastUpdatedIniFile);
	__declspec(dllexport) bool UCLogger_IsTargetCaller(LPCTSTR filter, LPCTSTR file, int line, LPCTSTR function);
	__declspec(dllexport) size_t UCLogger_LoadIniSection(TCHAR* pSectionName, ProcessIniKey pProcessIniKey, void* pContext);
	__declspec(dllexport) bool UCLogger_LoadConfiguration(LPCTSTR pSectionName);
#ifdef __cplusplus
}
#endif


// --------------------------------------------------
// Public Macros (Normally you should use these macros.)
// --------------------------------------------------
#define UC_LOG(filter, format, ...) \
	do { \
		if (UCLogger_IsTarget(filter) == false) break; \
		TCHAR _____log[1024];\
		ZeroMemory(_____log, sizeof(_____log));\
		UCLogger_MakeLog(filter, _T(__FILE__), __LINE__, _T(__FUNCTION__), _____log, _countof(_____log), format, __VA_ARGS__);\
		UCLogger_DebugLog(filter, _____log);\
	} while (0);

#define UC_LOG_STACK(filter, format, ...) \
	do { \
		if (UCLogger_IsTarget(filter) == false) break; \
		TCHAR _____formatEx[256];\
		ZeroMemory(_____formatEx, sizeof(_____formatEx));\
		_tcscat_s(_____formatEx, _T(">> CallStack: "));\
		_tcscat_s(_____formatEx, format);\
		TCHAR _____log[1024];\
		ZeroMemory(_____log, sizeof(_____log));\
		UCLogger_MakeLog(filter, _T(__FILE__), __LINE__, _T(__FUNCTION__), _____log, _countof(_____log), _____formatEx, __VA_ARGS__);\
		UCLogger_DebugLog(filter, _____log);\
		UCLogger_DebugLogStack(filter, nullptr, _T(__FILE__), __LINE__, _T(__FUNCTION__));\
		ZeroMemory(_____log, sizeof(_____log));\
		UCLogger_MakeLog(filter, _T(__FILE__), __LINE__, _T(__FUNCTION__), _____log, _countof(_____log), _T("<< CallStack"));\
		UCLogger_DebugLog(filter, _____log);\
	} while (0);


#ifdef UC_DEBUG_SELF_ENABLE
#define UC_DEBUG_SELF(format, ...) \
	do {\
		if (GetPrivateProfileInt(_T("SelfDebug"), _T(__FUNCTION__), 0, _T(INVESTIGATION_INI_FILE_PATH)) == 0) break;\
		TCHAR _____log[1024];\
		ZeroMemory(_____log, sizeof(_____log));\
		Investigation_MakeLog(_T("UC_DEBUG_SELF"), _T(__FILE__), __LINE__, _T(__FUNCTION__), _____log, _countof(_____log), format, __VA_ARGS__);\
		OutputDebugString(_____log);\
	} while (0);
#else
#define UC_DEBUG_SELF(format, ...) ;
#endif


#define UC_DEBUG_LOG_DECLARE_FILTER(filter, expression) \
	LPCTSTR _____dummy_##filter=nullptr,##filter = (expression) ? _T(#filter) : nullptr;

#define UC_MAKE_BIT_STRING(unsignedValue, buffer) \
	TCHAR _____dummy_##buffer,##buffer[BUFSIZ];\
	_____dummy_##buffer = _T('\0');\
	ZeroMemory(##buffer, sizeof(##buffer));\
	Investigation_MakeBitsString(##unsignedValue, sizeof(##unsignedValue), ##buffer, _countof(##buffer));

#define UC_PARAM_INT(key) \
	(GetPrivateProfileInt(_T("Parameters"), key, 0, _T(INVESTIGATION_INI_FILE_PATH)))

#define UC_PARAM_STRING(key, buffer) \
	TCHAR _____dummy_##buffer,##buffer[BUFSIZ];\
	_____dummy_##buffer = _T('\0');\
	ZeroMemory(##buffer, sizeof(##buffer));\
	GetPrivateProfileString(_T("Parameters"), key, _T(""), buffer, BUFSIZ - 1, _T(INVESTIGATION_INI_FILE_PATH));

#define UC_TARGET_CALLER(filter) \
	(UCLogger_IsTargetCaller(filter, _T(__FILE__), __LINE__, _T(__FUNCTION__)))

