// UC.Logger.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "UC.Logger.h"
#include <stdio.h>
#include <winbase.h>
#include <WinNls.h>

#include <time.h>
#include <sys/stat.h>

#pragma warning(push)
#pragma warning(disable:4091)
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#pragma warning(pop)

#include "Shlwapi.h"
#pragma comment(lib, "Shlwapi.lib")

#include <psapi.h>
#pragma comment ( lib, "psapi.lib" )

// --------------------------------------------------
// Definitions
// --------------------------------------------------
const size_t LARGE_BUFFER_SIZE = 1024;

#if !defined(_IMAGEHLP_SOURCE_) && defined(_IMAGEHLP64)
#define StackType DWORD64
#define StackUnitSize DWORD64
#define SymbolDisplacement DWORD64
#define LineDisplacement DWORD
#define SymbolAddress DWORD64
#else
#define StackType PVOID
#define StackUnitSize DWORD
#define SymbolDisplacement DWORD
#define LineDisplacement DWORD
#define SymbolAddress DWORD
#endif

typedef struct {
	int depth;
	TCHAR moduleName[BUFSIZ];
	TCHAR symbolName[BUFSIZ];
	SymbolAddress address;
	TCHAR fileName[MAX_PATH];
	DWORD lineNumber;
} FrameData;

typedef HRESULT(*ProcessFrameData)(LPCTSTR filter, LPCTSTR filterEx, LPCTSTR file, int line, LPCTSTR function, FrameData& frameData);

// --------------------------------------------------
// Global variables (only in INVESTIGATION.cpp file.)
// --------------------------------------------------
static Logger loggers[5] = { NULL, NULL, NULL, NULL, NULL };
static TCHAR cleanupFilter[256];
static TCHAR targetProcessNames[2014];
static DWORD targetProcessIds[32];

// --------------------------------------------------
// Private function declarations (only in INVESTIGATION.cpp file.)
// --------------------------------------------------
static void CleanupSymbols();
static void InitializeSymbols(HANDLE& process, LPCTSTR filter, DWORD& stackSize);
static bool GetProcessName(LPTSTR pBuffer, size_t countOfBuffer);
static void ManageProcess();
static int GetTargetProcessIdIndex(DWORD currentProcessId);
static bool IsTargetFilter(LPCTSTR filter, LPTSTR pFilters, size_t countOfFilter);
static bool IsTargetProcess();
static void OutputDebugStringToDebugView(LPCTSTR filter, LPCTSTR log);
static void OutputDebugStringToFile(LPCTSTR filter, LPCTSTR log);
static void PrepareString(const CHAR* srcBuffer, LPTSTR dstBuffer, size_t dstBufferLength);
static HRESULT ProcessStackData(LPCTSTR filter, LPCTSTR filterEx, LPCTSTR file, int line, LPCTSTR function, ProcessFrameData pProcess);
static HRESULT WriteFrameData(LPCTSTR filter, LPCTSTR filterEx, LPCTSTR file, int line, LPCTSTR function, FrameData& frameData);
static HRESULT IsCaller(LPCTSTR filter, LPCTSTR filterEx, LPCTSTR file, int line, LPCTSTR function, FrameData& frameData);
static bool ProcessIniKeyForFilters(TCHAR *pIniKey, void* pContext);

// --------------------------------------------------
// Public function definitions
// --------------------------------------------------
void UCLogger_ManageLogger(Logger logger, bool add)
{
	// Search Indexes
	int emptyIndex = -1;	// To add logger
	int existIndex = -1;	// To remove logger
	UC_DEBUG_SELF(_T("logger=0x%p, add=%d"), logger, add);
	for (size_t i = 0; i < _countof(loggers); i++)
	{
		UC_DEBUG_SELF(_T("loggers[%Iu]=0x%p"), i, loggers[i]);
		if (loggers[i] == NULL && emptyIndex < 0)
		{
			emptyIndex = (int)i;
		}

		if (loggers[i] == logger && existIndex < 0)
		{
			existIndex = (int)i;
		}
	}

	if (add)
	{
		// Add logger
		if (0 <= existIndex)
		{
			// Already exists
			UC_DEBUG_SELF(_T("Already exists at loggers[%d](=0x%p)"), existIndex, loggers[existIndex]);
			return;
		}
		if (emptyIndex < 0)
		{
			// No empty index
			UC_DEBUG_SELF(_T("No empty indexes"));
			return;
		}
		// Regist logger
		loggers[emptyIndex] = logger;
		UC_DEBUG_SELF(_T("SET loggers[%d] = logger(=0x%p)"), emptyIndex, logger);
	}
	else
	{
		// Remove logger
		if (existIndex < 0)
		{
			// Not exists
			UC_DEBUG_SELF(_T("Not exists"));
			return;
		}
		loggers[existIndex] = nullptr;
		UC_DEBUG_SELF(_T("DEL loggers[%d] = null"), existIndex);
	}
}

size_t UCLogger_LoadIniSection(TCHAR* pSectionName, ProcessIniKey pProcessIniKey, void* pContext)
{
	TCHAR buffer[BUFSIZ];
	ZeroMemory(buffer, sizeof(buffer));
	DWORD totalLength = GetPrivateProfileString(pSectionName, nullptr, _T(""), buffer, _countof(buffer), _T(UC_LOGGER_INI_FILE_PATH));
	UC_DEBUG_SELF(_T("totalLength=%lu"), totalLength);
	if (totalLength == 0)
	{
		return 0;
	}

	TCHAR* pKey = buffer;
	TCHAR* pEndOfKeys = &buffer[_countof(buffer) - 1];

	UC_DEBUG_SELF(_T("pKey=0x%p, pEndOfKeys=0x%p, diff=%Iu"), pKey, pEndOfKeys, pEndOfKeys - pKey);

	UC_DEBUG_SELF(_T(">> while loop"));
	size_t count = 0;
	while (true)
	{
		size_t keyLength = _tcslen(pKey);
		TCHAR* pNextKey = pKey + keyLength + 1;
		UC_DEBUG_SELF(_T("   pKey(0x%p)=[%s], pNextKey(0x%p), diff=%Iu"), pKey, pKey, pNextKey, pEndOfKeys - pKey);
		if (keyLength == 0 || pEndOfKeys <= pNextKey)
		{
			break;
		}

		if (pProcessIniKey(pKey, pContext))
		{
			count++;
		}

		pKey = pNextKey;
	}
	UC_DEBUG_SELF(_T("<< while loop"));

	return count;
}

bool UCLogger_IsTarget(LPCTSTR filter)
{
	if (filter == nullptr)
	{
		return false;
	}
	if (_tcslen(filter) == 0)
	{
		return false;
	}

	static DWORD lastTickCount = 0;
	static time_t lastUpdatedIniFile = 0;
	static TCHAR filters[BUFSIZ];
	static size_t countOfFilters = 0;

	DWORD lastTicCountBackup = lastTickCount;
	if (UCLogger_WatchIniFileLastUpdated(lastTickCount, lastUpdatedIniFile))
	{
		// DbgView Logger
		bool addDbgView = GetPrivateProfileInt(_T("Output"), _T("DebugView"), 0, _T(UC_LOGGER_INI_FILE_PATH)) == 0 ? false : true;
		UCLogger_ManageLogger(OutputDebugStringToDebugView, addDbgView);

		// File Logger
		TCHAR logFilePath[MAX_PATH];
		ZeroMemory(logFilePath, sizeof(logFilePath));
		DWORD length = GetPrivateProfileString(_T("Output"), _T("File"), _T(""), logFilePath, sizeof(logFilePath), _T(UC_LOGGER_INI_FILE_PATH));
		bool addFile = (0 < length) ? true : false;
		UCLogger_ManageLogger(OutputDebugStringToFile, addFile);

		// Filter
		ZeroMemory(filters, sizeof(filters));
		countOfFilters = UCLogger_LoadIniSection(_T("Filters"), ProcessIniKeyForFilters, filters);

		// Process
		ManageProcess();
	}
	UC_DEBUG_SELF(_T("lastTickCount = %lu -> %lu"), lastTicCountBackup, lastTickCount);

	if (IsTargetFilter(filter, filters, countOfFilters) == false)
	{
		return false;
	}

	if (IsTargetProcess() == false)
	{
		return false;
	}

	return true;
}

void UCLogger_MakeLog(LPCTSTR filter, LPCTSTR file, int line, LPCTSTR function, LPTSTR log, size_t countOfLog, LPCTSTR format, ...)
{
	// Position (file, line, function)
	DWORD threadId = GetCurrentThreadId();
	TCHAR position[1024];
	ZeroMemory(position, sizeof(position));
	_stprintf_s(position, _countof(position), _T("(TID:%lu)[%s:(%d)/%s] "), threadId, PathFindFileName(file), line, function);

	// Message
	TCHAR message[1024];
	ZeroMemory(message, sizeof(message));
	va_list args;
	va_start(args, format);
	int result = _vstprintf_s(message, _countof(message), format, args);
	va_end(args);

	// Make Log
	ZeroMemory(log, sizeof(log));
	_tcscat_s(log, countOfLog, filter);
	_tcscat_s(log, countOfLog, position);
	_tcscat_s(log, countOfLog, message);
}

void UCLogger_DebugLog(LPCTSTR filter, LPCTSTR log)
{
	// Write Log
	for (size_t i = 0; i < _countof(loggers); i++)
	{
		if (loggers[i] == NULL)
			break;
		loggers[i](filter, log);
	}
}

void UCLogger_DebugLogStack(LPCTSTR filter, LPCTSTR filterEx, LPCTSTR file, int line, LPCTSTR function)
{
	ProcessStackData(filter, filterEx, file, line, function, WriteFrameData);
}

bool UCLogger_IsTargetCaller(LPCTSTR filter, LPCTSTR file, int line, LPCTSTR function)
{
	if (UCLogger_IsTarget(filter) == false)
	{
		UC_DEBUG_SELF(_T("Investigation_IsTarget(\"%s\") == false"), filter);
		return false;
	}
	HRESULT hr = ProcessStackData(filter, nullptr, file, line, function, IsCaller);
	return (hr == S_OK ? true : false);
}

bool UCLogger_MakeBitsString(unsigned long long unsignedValue, size_t byteSizeOfValue, LPTSTR pBuffer, size_t countOfBuffer)
{
	if (pBuffer == nullptr)
	{
		return false;
	}
	const size_t BITCOUNT_OF_BYTE = 8;
	ZeroMemory(pBuffer, countOfBuffer * sizeof(TCHAR));
	size_t allBitsCount = BITCOUNT_OF_BYTE * byteSizeOfValue;
	if (countOfBuffer < allBitsCount)
	{
		return false;
	}

	unsigned long long tmpUnsignedValue = unsignedValue;
	for (size_t i = 0; i < allBitsCount; i++)
	{
		size_t separatorCount = (allBitsCount - i - 1) / BITCOUNT_OF_BYTE;
		size_t index = (allBitsCount - 1) - i + separatorCount;
		bool requiredSeparator = (((i + 1) % BITCOUNT_OF_BYTE) == 0 && 1 <= index) ? true : false;
		if (requiredSeparator)
		{
			pBuffer[index - 1] = _T('-');
		}

		pBuffer[index] = (tmpUnsignedValue % 2 == 0) ? _T('0') : _T('1');
		tmpUnsignedValue = tmpUnsignedValue / 2;
	}
	return true;
}

bool UCLogger_WatchIniFileLastUpdated(DWORD& lastTickCount, time_t& lastUpdatedIniFile)
{
#ifdef UC_DEBUG_LOG_INI_WATCH_MSEC
	// Log buffer
	TCHAR log[1024] = { 0 };
	ZeroMemory(log, sizeof(log));

	// Check Watch Interval by TickCount
	DWORD currentTickCount = GetTickCount();
	if (currentTickCount - lastTickCount < DEBUG_LOG_INI_WATCH_MSEC)
	{
		UC_DEBUG_SELF(_T("IniFile(%s) currentTickCount(=%lu) - lastTickCount(=%lu) = %lu < DEBUG_LOG_INI_WATCH_MSEC(=%lu)"),
			_T(INVESTIGATION_INI_FILE_PATH), currentTickCount, lastTickCount, currentTickCount - lastTickCount, DEBUG_LOG_INI_WATCH_MSEC);
		return false;
	}
	// Update Tick Count
	lastTickCount = currentTickCount;
	UC_DEBUG_SELF(_T("IniFile(%s) Update lastTickCount(=%lu)"), _T(INVESTIGATION_INI_FILE_PATH), lastTickCount);

	// Get Ini file's last updated time.
	struct stat st = { 0 };
	stat(INVESTIGATION_INI_FILE_PATH, &st);

	// Compare Init file's last updated time between getting time and on memory time.
	if (st.st_mtime <= lastUpdatedIniFile)
	{
		UC_DEBUG_SELF(_T("IniFile(%s) Not Updated"), _T(INVESTIGATION_INI_FILE_PATH));
		return false;
	}
	// Update last updated time on memory.
	lastUpdatedIniFile = st.st_mtime;

	// Logging
#ifdef DEBUG_SELF_ENABLE
	struct tm tm = { 0 };
	errno_t error = localtime_s(&tm, &st.st_mtime);
	TCHAR time[128] = { 0 };
	ZeroMemory(time, sizeof(time));
	_tasctime_s(time, _countof(time), &tm);
	UC_DEBUG_SELF(_T("IniFile(%s) Updated at [%s]"), _T(INVESTIGATION_INI_FILE_PATH), time);
#endif

	return true;

#else
	// If 'DEBUG_LOG_INI_WATCH_MSEC' was not defined,
	// Investigation module read only one time at first time.
	if (lastTickCount == 0)
	{
		lastTickCount = GetTickCount();
		return true;
	}
	else
	{
		return false;
	}
#endif
}

static bool LoadConfigurationOutput()
{
	// DbgView Logger
	bool addDbgView = GetPrivateProfileInt(_T("Output"), _T("DebugView"), 0, _T(UC_LOGGER_INI_FILE_PATH)) == 0 ? false : true;
	UCLogger_ManageLogger(OutputDebugStringToDebugView, addDbgView);
	return true;
}

static bool LoadConfigurationSelfDebug()
{
	return true;
}

static bool LoadConfigurationStackTrace()
{
	return true;
}

static bool LoadConfigurationRoutes()
{
	return true;
}

static bool LoadConfigurationFilters()
{
	return true;
}

static bool LoadConfigurationProcesses()
{
	return true;
}

bool UCLogger_LoadConfiguration(LPCTSTR pSectionName)
{
	if (pSectionName == nullptr || _tcsicmp(_T("Output"), pSectionName) == 0)
	{
		LoadConfigurationOutput();
	}

	if (pSectionName == nullptr || _tcsicmp(_T("SelfDebug"), pSectionName) == 0)
	{
		LoadConfigurationSelfDebug();
	}

	if (pSectionName == nullptr || _tcsicmp(_T("StackTrace"), pSectionName) == 0)
	{
		LoadConfigurationStackTrace();
	}

	if (pSectionName == nullptr || _tcsicmp(_T("Routes"), pSectionName) == 0)
	{
		LoadConfigurationRoutes();
	}

	//if (pSectionName == nullptr || _tcsicmp(_T("Parameters"), pSectionName) == 0)
	//{
	//	LoadConfigurationParameters();
	//}

	if (pSectionName == nullptr || _tcsicmp(_T("Filters"), pSectionName) == 0)
	{
		LoadConfigurationFilters();
	}

	if (pSectionName == nullptr || _tcsicmp(_T("Processes"), pSectionName) == 0)
	{
		LoadConfigurationProcesses();
	}

	return true;
}

// --------------------------------------------------
// Private function definitions (only in INVESTIGATION.cpp file.)
// --------------------------------------------------
static void CleanupSymbols()
{
	SymCleanup(GetCurrentProcess());

	TCHAR log[1024];
	ZeroMemory(log, sizeof(log));
	UCLogger_MakeLog(cleanupFilter, _T(__FILE__), __LINE__, _T(__FUNCTION__), log, _countof(log), _T("Symbols cleanup completed."));
	UCLogger_DebugLog(cleanupFilter, log);
}

static void InitializeSymbols(HANDLE& process, LPCTSTR filter, DWORD& stackSize)
{
	// Read Stack Size
	stackSize = GetPrivateProfileInt(_T("StackTrace"), _T("Size"), 20, _T(UC_LOGGER_INI_FILE_PATH));

	// Read Symbol Path
	char symbolPathA[1024];
	ZeroMemory(symbolPathA, sizeof(symbolPathA));
	GetPrivateProfileStringA("StackTrace", "SymbolPath", "C:\\Symbols", symbolPathA, sizeof(symbolPathA), UC_LOGGER_INI_FILE_PATH);

	// Initialize(Load) Symbol files
	SymInitialize(process, symbolPathA, TRUE);
	DWORD64 result = SymLoadModule(process, NULL, NULL, NULL, 0, 0);

	// Register a function which cleans up symbols when current process exit.
	_tcscpy_s(cleanupFilter, filter);
	atexit(CleanupSymbols);

	// Write Log
	TCHAR log[1024];
	ZeroMemory(log, sizeof(log));
	UCLogger_MakeLog(filter, _T(__FILE__), __LINE__, _T(__FUNCTION__), log, _countof(log), _T("InitializeSymbols(...) completed."));
	UCLogger_DebugLog(filter, log);
}

bool GetProcessName(LPTSTR pBuffer, size_t countOfBuffer)
{
	ZeroMemory(pBuffer, countOfBuffer * sizeof(TCHAR));

	DWORD dwProcessId = ::GetCurrentProcessId();
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
	if (hProcess == nullptr)
	{
		return false;
	}

	::GetModuleBaseName(hProcess, NULL, pBuffer, (DWORD)countOfBuffer);

	::CloseHandle(hProcess);

	return true;
}

void ManageProcess()
{
	ZeroMemory(targetProcessNames, sizeof(targetProcessNames));
	GetPrivateProfileString(_T("Processes"), _T("Target"), _T(""), targetProcessNames, sizeof(targetProcessNames), _T(UC_LOGGER_INI_FILE_PATH));
	if (_tcslen(targetProcessNames) == 0)
	{
		UC_DEBUG_SELF(_T("Target process names are not Specified."));
		return;
	}
	_tcsupr_s(targetProcessNames);
	UC_DEBUG_SELF(_T("Target process names = [%s]"), targetProcessNames);

	TCHAR processName[BUFSIZ];
	ZeroMemory(processName, sizeof(processName));
	GetProcessName(processName, _countof(processName));
	_tcsupr_s(processName);
	UC_DEBUG_SELF(_T("Current process name = [%s]"), processName);
	if (_tcsstr(targetProcessNames, processName) == nullptr)
	{
		// Not register
		UC_DEBUG_SELF(_T("Not HIT"));
		return;
	}

	// Check existing process ID
	DWORD currentProcessId = GetCurrentProcessId();
	UC_DEBUG_SELF(_T("Current Process ID = %lu"), currentProcessId);
	if (0 <= GetTargetProcessIdIndex(currentProcessId))
	{
		// Already registered
		UC_DEBUG_SELF(_T("Already registered"));
		return;
	}

	// Register the current process ID
	bool alreadyRegistered = false;
	for (size_t i = 0; i < _countof(targetProcessIds); i++)
	{
		if ((targetProcessIds[i] == 0) && (alreadyRegistered == false))
		{
			targetProcessIds[i] = currentProcessId;
			UC_DEBUG_SELF(_T("targetProcessIds[%Iu]=%lu <- !!NEW!!"), i, targetProcessIds[i]);
			alreadyRegistered = true;
		}
		else
		{
			UC_DEBUG_SELF(_T("targetProcessIds[%Iu]=%lu"), i, targetProcessIds[i]);
		}
	}
}

int GetTargetProcessIdIndex(DWORD currentProcessId)
{
	for (size_t i = 0; i < _countof(targetProcessIds); i++)
	{
		if (targetProcessIds[i] == currentProcessId)
		{
			UC_DEBUG_SELF(_T("Registered index = %d"), i);
			return (int)i;
		}
	}

	UC_DEBUG_SELF(_T("Not exist"));
	return -1;
}

static bool IsTargetFilter(LPCTSTR filter, LPTSTR pFilters, size_t countOfFilter)
{
	UC_DEBUG_SELF(_T("filter=[%s], pFilters=0x%p, countOfFilter=%Iu"), filter, pFilters, countOfFilter);
	if (pFilters == nullptr)
	{
		UC_DEBUG_SELF(_T("[%s] is target. (pFilters == nullptr)"));
		return true;
	}
	if (_tcslen(pFilters) == 0)
	{
		UC_DEBUG_SELF(_T("[%s] is target. (_tcslen(pFilters) == 0)"));
		return true;
	}

	TCHAR* pCursor = pFilters;
	for (size_t i = 0; i < countOfFilter; i++)
	{
		UC_DEBUG_SELF(_T("i=%Iu, [%s]"), i, pCursor);
		if (_tcscmp(pCursor, filter) == 0)
		{
			UC_DEBUG_SELF(_T("[%s] is target. (Hit)"), filter);
			return true;
		}
		pCursor = pCursor + _tcslen(pCursor) + 1;
	}
	UC_DEBUG_SELF(_T("[%s] is *NOT* target. (Not Hit)"), filter);
	return false;
}

static bool IsTargetProcess()
{
	// If Not specified, all processes are target.
	if (_tcslen(targetProcessNames) == 0)
	{
		UC_DEBUG_SELF(_T("All processes are target."));
		return true;
	}


	if (0 <= GetTargetProcessIdIndex(GetCurrentProcessId()))
	{
		UC_DEBUG_SELF(_T("Current process is a target."));
		return true;
	}

	UC_DEBUG_SELF(_T("Current process is NOT a target."));
	return false;
}

static void PrepareString(const CHAR* srcBuffer, LPTSTR dstBuffer, size_t dstBufferLength)
{
#ifdef _MBCS
	strcpy_s((LPSTR)dstBuffer, dstBufferLength, srcBuffer);
#else
	MultiByteToWideChar(CP_OEMCP, MB_PRECOMPOSED, (LPCCH)srcBuffer, (int)strlen(srcBuffer), (LPWSTR)dstBuffer, (int)dstBufferLength);
#endif
}

static HRESULT WriteFrameData(LPCTSTR filter, LPCTSTR filterEx, LPCTSTR file, int line, LPCTSTR function, FrameData& frameData)
{
	// Write
	TCHAR log[1024];
	ZeroMemory(log, sizeof(log));
	UCLogger_MakeLog(filterEx ? filterEx : filter, file, line, function, log, _countof(log), _T("   %i: [%s] %s - 0x%0X(%s,Line:%d)"),
		frameData.depth,
		frameData.moduleName,
		frameData.symbolName,
		frameData.address,
		frameData.fileName,
		frameData.lineNumber);
	UCLogger_DebugLog(filter, log);
	return S_FALSE;
}

static HRESULT ProcessStackData(LPCTSTR filter, LPCTSTR filterEx, LPCTSTR file, int line, LPCTSTR function, ProcessFrameData pProcess)
{
	static bool isInitialized = false;
	static DWORD stackSize = 0;

	HANDLE process = GetCurrentProcess();

	// Initialize
	if (isInitialized == false)
	{
		isInitialized = true;
		InitializeSymbols(process, filter, stackSize);
	}

	// Stack
	StackType* pStack = (StackType*)malloc(sizeof(StackType) * stackSize);
	if (pStack == nullptr)
	{
		return E_POINTER;
	}
	unsigned short frames = CaptureStackBackTrace(0, stackSize, (PVOID*)pStack, NULL);

	// Output each stack information
	unsigned short lastFrameIndex = (frames == 0) ? 0 : (frames - 1);
	const unsigned int SKIP_FRAME_COUNT = 2;
	HRESULT hr = S_OK;
	for (unsigned int i = SKIP_FRAME_COUNT; i < lastFrameIndex; i++)
	{
		const size_t BUFFER_LENGTH = 256;
		FrameData frameData = { 0 };

		// Symbol Name
		IMAGEHLP_SYMBOL * pImageSymbol;
		char imageSymbolBuffer[sizeof(IMAGEHLP_SYMBOL) + MAX_PATH] = { 0 };
		pImageSymbol = (IMAGEHLP_SYMBOL*)imageSymbolBuffer;
		pImageSymbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
		pImageSymbol->MaxNameLength = MAX_PATH;
		SymbolDisplacement symbolDisplacement = 0;
		SymGetSymFromAddr(process, (StackUnitSize)pStack[i], &symbolDisplacement, pImageSymbol);
		PrepareString(pImageSymbol->Name, frameData.symbolName, _countof(frameData.symbolName));

		// Module Name
		IMAGEHLP_MODULE imageModule = { sizeof(IMAGEHLP_MODULE) };
		SymGetModuleInfo(process, (StackUnitSize)pStack[i], &imageModule);
		PrepareString(imageModule.ModuleName, frameData.moduleName, _countof(frameData.moduleName));

		// File Name & Line Number
		LineDisplacement lineDisplacement = 0;
		IMAGEHLP_LINE imageLine = { sizeof(IMAGEHLP_LINE) };
		SymGetLineFromAddr(process, (StackUnitSize)pStack[i], &lineDisplacement, &imageLine);
		PrepareString((imageLine.FileName == nullptr ? "<Not Found>" : PathFindFileNameA(imageLine.FileName)), frameData.fileName, _countof(frameData.fileName));

		// Other frame information (depth, address, line number)
		frameData.depth = frames - i - 1;
		frameData.address = pImageSymbol->Address;
		frameData.lineNumber = imageLine.LineNumber;

		// Process for a frame data
		hr = pProcess(filter, filterEx, file, line, function, frameData);
		// S_OK ... break, but result is good.
		// S_FALSE ... continue.
		// E_XXX   ... break. and result is bad.
		if (hr == S_OK || FAILED(hr))
		{
			break;
		}
	}
	free(pStack);

	return hr;
}

typedef struct {
	TCHAR key[BUFSIZ];
	FrameData frame;

} Routes;

static void InitializeRoutes()
{
	TCHAR buffer[LARGE_BUFFER_SIZE];
	ZeroMemory(buffer, sizeof(buffer));
	DWORD size = GetPrivateProfileSection(_T("Routes"), buffer, _countof(buffer), _T(UC_LOGGER_INI_FILE_PATH));


}

inline TCHAR* ScanParameter(LPCTSTR filter, TCHAR* pStart, TCHAR* buffer, size_t bufferSize)
{
	UC_DEBUG_SELF(_T("pStart=0x%p[%s]"), pStart, pStart);

	ZeroMemory(buffer, bufferSize);
	TCHAR* pSeparator = _tcschr(pStart, _T('/'));
	UC_DEBUG_SELF(_T("pSeparator=0x%p,[%s]"), pSeparator, pSeparator);
	if (pSeparator == nullptr)
	{
		_tcscpy_s(buffer, bufferSize / sizeof(TCHAR), pStart);
		UC_DEBUG_SELF(_T("buffer=[%s]"), buffer);
		return nullptr;
	}
	size_t count = pSeparator - pStart;
	UC_DEBUG_SELF(_T("count=%Iu"), count);
	_tcsncpy_s(buffer, bufferSize / sizeof(TCHAR), pStart, count);
	UC_DEBUG_SELF(_T("buffer=[%s]"), buffer);
	return pSeparator + 1;
}

static bool ProcessIniKeyForFilters(TCHAR *pIniKey, void* pContext)
{
	UC_DEBUG_SELF(_T("pIniKey=[%s], pContext=0x%p"), pIniKey, pContext);
	if (pContext == nullptr)
	{
		return false;
	}
	TCHAR* pFilters = (TCHAR*)pContext;

	bool enable = (GetPrivateProfileInt(_T("Filters"), pIniKey, 0, _T(UC_LOGGER_INI_FILE_PATH)) == 0) ? false : true;
	UC_DEBUG_SELF(_T("enable=%d"), enable);
	if (enable == false)
	{
		return false;
	}

	TCHAR* pCursor = pFilters;
	while (*pCursor != _T('\0'))
	{
		UC_DEBUG_SELF(_T("pCursor=0x%p,[%s]"), pCursor, pCursor);
		pCursor += (_tcslen(pCursor) + 1);
	}

#pragma warning(push)
#pragma warning(disable:4996)
	_tcscpy(pCursor, pIniKey);
#pragma warning(pop)
	UC_DEBUG_SELF(_T("pCursor=0x%p,[%s] copied."), pCursor, pCursor);
	return true;
}

static HRESULT IsCaller(LPCTSTR filter, LPCTSTR filterEx, LPCTSTR file, int line, LPCTSTR function, FrameData& frameData)
{
	//static bool isInitialized = false;
	//if (isInitialized == false)
	//{
	//	InitializeRoutes();
	//}
	// Make key string (ex "XXX.cpp/YYY::ZZZ")
	TCHAR keyBuffer[BUFSIZ];
	ZeroMemory(keyBuffer, sizeof(keyBuffer));
	_stprintf_s(keyBuffer, _T("%s/%s"), file, function);
	UC_DEBUG_SELF(_T("Key=[%s]"), keyBuffer);

	// Get value string
	TCHAR valueBuffer[BUFSIZ];
	ZeroMemory(valueBuffer, sizeof(valueBuffer));
	DWORD length = GetPrivateProfileString(_T("Routes"), keyBuffer, _T(""), valueBuffer, sizeof(valueBuffer), _T(UC_LOGGER_INI_FILE_PATH));
	UC_DEBUG_SELF(_T("Value=[%s]"), valueBuffer);
	// 1/shr1090u64/Security.cpp/shr1090::CheckTagSecurity/127,...

	if (_tcslen(valueBuffer) == 0)
	{
		UC_DEBUG_SELF(_T("return E_NOT_SET, Caller is not configured.."));
		return E_NOT_SET;
	}

	TCHAR* pCursor = valueBuffer;

	// Depth
	TCHAR depthBuffer[BUFSIZ];
	pCursor = ScanParameter(filter, pCursor, depthBuffer, sizeof(depthBuffer));
	int depth = _tstoi(depthBuffer);
	if (frameData.depth != depth)
	{
		// Not target depth. But require to continue the process.
		UC_DEBUG_SELF(_T("return S_FALSE, depth ... (frame=%d, param=%d[%s])"), frameData.depth, depth, depthBuffer);
		return S_FALSE;
	}

	// Module name
	TCHAR moduleName[BUFSIZ];
	pCursor = ScanParameter(filter, pCursor, moduleName, sizeof(moduleName));
	if (_tcsicmp(frameData.moduleName, moduleName) != 0)
	{
		// Not target caller. Break the process.
		UC_DEBUG_SELF(_T("return E_FAIL, module name ... (frame=%s, param=%s)"), frameData.moduleName, moduleName);
		return E_FAIL;
	}

	// File name
	TCHAR fileName[BUFSIZ];
	pCursor = ScanParameter(filter, pCursor, fileName, sizeof(fileName));
	if (_tcsicmp(frameData.fileName, fileName) != 0)
	{
		// Not target caller. Break the process.
		UC_DEBUG_SELF(_T("return E_FAIL, file name ... (frame=%s, param=%s)"), frameData.fileName, fileName);
		return E_FAIL;
	}

	// Function name
	TCHAR functionName[BUFSIZ];
	pCursor = ScanParameter(filter, pCursor, functionName, sizeof(functionName));
	if (_tcsicmp(frameData.symbolName, functionName) != 0)
	{
		// Not target caller. Break the process.
		UC_DEBUG_SELF(_T("return E_FAIL, file name ... (frame=%s, param=%s)"), frameData.symbolName, functionName);
		return E_FAIL;
	}

	TCHAR lineNumberBuffer[BUFSIZ];
	pCursor = ScanParameter(filter, pCursor, lineNumberBuffer, sizeof(lineNumberBuffer));
	int lineNumber = _tstoi(lineNumberBuffer);
	if (lineNumber != 0 && frameData.lineNumber != lineNumber)
	{
		// Not target caller. Break the process.
		UC_DEBUG_SELF(_T("return E_FAIL, lineNumber"));
		return E_FAIL;
	}

	UC_DEBUG_SELF(_T("depth=%d, moduleName=[%s], fileName=[%s], functionName=[%s], lineNumber=%d"), depth, moduleName, fileName, functionName, lineNumber);

	// Target Caller
	UC_DEBUG_SELF(_T("return S_OK"));
	return S_OK;
}

// --------------------------------------------------
// Out put debug string functions
// --------------------------------------------------
static void OutputDebugStringToDebugView(LPCTSTR filter, LPCTSTR log)
{
	TCHAR buffer[2014];
	ZeroMemory(buffer, sizeof(buffer));
	_stprintf_s(buffer, _T("%s\n"), log);
	OutputDebugString(buffer);
}

static void OutputDebugStringToFile(LPCTSTR filter, LPCTSTR log)
{
	static TCHAR logFilePath[MAX_PATH] = { 0 };
	static TCHAR mutexName[BUFSIZ];
	//UC_DEBUG_SELF(_T("logFilePath=[%s]"), logFilePath);
	if (_tcslen(logFilePath) == 0)
	{
		// Get log file path
		TCHAR tmpLogFilePath[MAX_PATH] = { 0 };
		ZeroMemory(tmpLogFilePath, sizeof(tmpLogFilePath));
		DWORD length = GetPrivateProfileString(_T("Output"), _T("File"), _T(""), tmpLogFilePath, sizeof(tmpLogFilePath), _T(UC_LOGGER_INI_FILE_PATH));
		if (_tcslen(tmpLogFilePath) == 0)
		{
			//UC_DEBUG_SELF(_T("return, _tcslen(tmpLogFilePath) == 0"));
			return;
		}
		ZeroMemory(logFilePath, sizeof(logFilePath));
		_stprintf_s(logFilePath, _T("%s"), tmpLogFilePath);
		//UC_DEBUG_SELF(_T("logFilePath=[%s]"), logFilePath);
		ZeroMemory(mutexName, sizeof(mutexName));
		_stprintf_s(mutexName, _T("INVESTIGATION-%s"), tmpLogFilePath);
	}

	// Create Mutex object & regist close mutex procedure at end of process.
	HANDLE hMutex = CreateMutex(NULL, FALSE, mutexName);
	WaitForSingleObject(hMutex, INFINITE);

	// Write log message.
	//FILE* pFile = _tfopen_s((logFilePath, _T("a"));
	FILE* pFile = nullptr;
	errno_t err = _tfopen_s(&pFile, logFilePath, _T("a"));
	//UC_DEBUG_SELF(_T("pFile=0x%p"), pFile);
	if (pFile != nullptr)
	{
		_fputts(log, pFile);
		_fputts(_T("\n"), pFile);
		_fflush_nolock(pFile);
		fclose(pFile);
	}

	// Release & Close Mutex handle
	ReleaseMutex(hMutex);
	CloseHandle(hMutex);
}

void WatchIniFileUpdate()
{

}
