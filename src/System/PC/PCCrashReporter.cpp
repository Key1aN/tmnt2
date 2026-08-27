#include "PCCrashReporter.hpp"

#include "PCTypedefs.hpp"

#include <DbgHelp.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>


namespace
{
    typedef BOOL(WINAPI* MiniDumpWriteDumpProc)(
        HANDLE,
        DWORD,
        HANDLE,
        MINIDUMP_TYPE,
        PMINIDUMP_EXCEPTION_INFORMATION,
        PMINIDUMP_USER_STREAM_INFORMATION,
        PMINIDUMP_CALLBACK_INFORMATION
    );

    static LPTOP_LEVEL_EXCEPTION_FILTER s_pPreviousFilter = nullptr;
    static volatile LONG s_handlingCrash = 0;
    static HANDLE s_hTrace = INVALID_HANDLE_VALUE;


    static void GetGameDirectory(char* path, size_t pathSize)
    {
        path[0] = '\0';
        DWORD length = GetModuleFileNameA(nullptr, path, static_cast<DWORD>(pathSize));
        if (!length || (length >= pathSize))
        {
            std::strcpy(path, ".\\");
            return;
        };

        char* pBackslash = std::strrchr(path, '\\');
        char* pSlash = std::strrchr(path, '/');
        char* pSeparator = pBackslash;
        if (!pSeparator || (pSlash && (pSlash > pSeparator)))
            pSeparator = pSlash;
        if (pSeparator)
            pSeparator[1] = '\0';
        else
            std::strcpy(path, ".\\");
    };


    static void BuildGamePath(char* path, size_t pathSize, const char* filename)
    {
        char directory[MAX_PATH] = {};
        GetGameDirectory(directory, COUNT_OF(directory));
        _snprintf_s(path, pathSize, _TRUNCATE, "%s%s", directory, filename);
    };


    static void BuildTimestampedGamePath(char* path,
                                         size_t pathSize,
                                         const char* prefix,
                                         const char* extension)
    {
        SYSTEMTIME time = {};
        GetLocalTime(&time);

        char filename[MAX_PATH] = {};
        _snprintf_s(filename,
                    COUNT_OF(filename),
                    _TRUNCATE,
                    "%s_%04u%02u%02u_%02u%02u%02u_%03u.%s",
                    prefix,
                    time.wYear,
                    time.wMonth,
                    time.wDay,
                    time.wHour,
                    time.wMinute,
                    time.wSecond,
                    time.wMilliseconds,
                    extension);

        BuildGamePath(path, pathSize, filename);
    };


    static void WriteAll(HANDLE hFile, const char* text)
    {
        if ((hFile == INVALID_HANDLE_VALUE) || !text)
            return;

        DWORD bytesWritten = 0;
        WriteFile(hFile,
                  text,
                  static_cast<DWORD>(std::strlen(text)),
                  &bytesWritten,
                  nullptr);
        FlushFileBuffers(hFile);
    };


    static void AppendTraceText(const char* text)
    {
        if (!text)
            return;

        if (s_hTrace != INVALID_HANDLE_VALUE)
        {
            DWORD bytesWritten = 0;
            WriteFile(s_hTrace,
                      text,
                      static_cast<DWORD>(std::strlen(text)),
                      &bytesWritten,
                      nullptr);
            return;
        };

        char path[MAX_PATH] = {};
        BuildGamePath(path, COUNT_OF(path), "TMNT2_Slashuur_trace.log");

        HANDLE hFile = CreateFileA(path,
                                   FILE_APPEND_DATA,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr,
                                   OPEN_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD bytesWritten = 0;
            WriteFile(hFile,
                      text,
                      static_cast<DWORD>(std::strlen(text)),
                      &bytesWritten,
                      nullptr);
            CloseHandle(hFile);
        };
    };


    static const char* ExceptionName(DWORD code)
    {
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT:     return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:        return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INVALID_OPERATION:     return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_ILLEGAL_INSTRUCTION:       return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:             return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:        return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:              return "EXCEPTION_INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:        return "EXCEPTION_INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:   return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:           return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW:             return "EXCEPTION_STACK_OVERFLOW";
        default:                                   return "UNKNOWN_EXCEPTION";
        };
    };


    static void WriteMiniDump(const char* path, EXCEPTION_POINTERS* pException)
    {
        HMODULE hDbgHelp = LoadLibraryA("dbghelp.dll");
        if (!hDbgHelp)
            return;

        MiniDumpWriteDumpProc pMiniDumpWriteDump =
            reinterpret_cast<MiniDumpWriteDumpProc>(GetProcAddress(hDbgHelp, "MiniDumpWriteDump"));

        if (pMiniDumpWriteDump)
        {
            HANDLE hFile = CreateFileA(path,
                                       GENERIC_WRITE,
                                       FILE_SHARE_READ,
                                       nullptr,
                                       CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL,
                                       nullptr);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                MINIDUMP_EXCEPTION_INFORMATION exceptionInfo = {};
                exceptionInfo.ThreadId = GetCurrentThreadId();
                exceptionInfo.ExceptionPointers = pException;
                exceptionInfo.ClientPointers = FALSE;

                MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
                    MiniDumpWithDataSegs |
                    MiniDumpWithHandleData |
                    MiniDumpWithIndirectlyReferencedMemory |
                    MiniDumpWithThreadInfo
                );

                pMiniDumpWriteDump(GetCurrentProcess(),
                                   GetCurrentProcessId(),
                                   hFile,
                                   dumpType,
                                   &exceptionInfo,
                                   nullptr,
                                   nullptr);
                CloseHandle(hFile);
            };
        };

        FreeLibrary(hDbgHelp);
    };


    static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* pException)
    {
        if (InterlockedExchange(&s_handlingCrash, 1) != 0)
            return EXCEPTION_EXECUTE_HANDLER;

        if (s_hTrace != INVALID_HANDLE_VALUE)
            FlushFileBuffers(s_hTrace);

        SYSTEMTIME time = {};
        GetLocalTime(&time);

        char logFilename[MAX_PATH] = {};
        char dumpFilename[MAX_PATH] = {};
        _snprintf_s(logFilename,
                    COUNT_OF(logFilename),
                    _TRUNCATE,
                    "TMNT2_crash_%04u%02u%02u_%02u%02u%02u_%03u.log",
                    time.wYear,
                    time.wMonth,
                    time.wDay,
                    time.wHour,
                    time.wMinute,
                    time.wSecond,
                    time.wMilliseconds);
        _snprintf_s(dumpFilename,
                    COUNT_OF(dumpFilename),
                    _TRUNCATE,
                    "TMNT2_crash_%04u%02u%02u_%02u%02u%02u_%03u.dmp",
                    time.wYear,
                    time.wMonth,
                    time.wDay,
                    time.wHour,
                    time.wMinute,
                    time.wSecond,
                    time.wMilliseconds);

        char logPath[MAX_PATH] = {};
        char dumpPath[MAX_PATH] = {};
        BuildGamePath(logPath, COUNT_OF(logPath), logFilename);
        BuildGamePath(dumpPath, COUNT_OF(dumpPath), dumpFilename);

        HANDLE hLog = CreateFileA(logPath,
                                  GENERIC_WRITE,
                                  FILE_SHARE_READ,
                                  nullptr,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);

        if (hLog != INVALID_HANDLE_VALUE)
        {
            EXCEPTION_RECORD* pRecord = (pException ? pException->ExceptionRecord : nullptr);
            CONTEXT* pContext = (pException ? pException->ContextRecord : nullptr);
            DWORD exceptionCode = (pRecord ? pRecord->ExceptionCode : 0);
            std::uintptr_t exceptionAddress =
                reinterpret_cast<std::uintptr_t>(pRecord ? pRecord->ExceptionAddress : nullptr);
            std::uintptr_t moduleBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
            std::uintptr_t exceptionRva =
                ((exceptionAddress >= moduleBase) ? (exceptionAddress - moduleBase) : 0);

            char text[4096] = {};
            _snprintf_s(text,
                        COUNT_OF(text),
                        _TRUNCATE,
                        "TMNT2 Playable Slashuur crash report\r\n"
                        "Time: %04u-%02u-%02u %02u:%02u:%02u.%03u\r\n"
                        "ProcessId: %lu\r\n"
                        "ThreadId: %lu\r\n"
                        "Exception: %s (0x%08lX)\r\n"
                        "ExceptionAddress: 0x%08lX\r\n"
                        "ModuleBase: 0x%08lX\r\n"
                        "ModuleRVA: 0x%08lX\r\n",
                        time.wYear,
                        time.wMonth,
                        time.wDay,
                        time.wHour,
                        time.wMinute,
                        time.wSecond,
                        time.wMilliseconds,
                        static_cast<unsigned long>(GetCurrentProcessId()),
                        static_cast<unsigned long>(GetCurrentThreadId()),
                        ExceptionName(exceptionCode),
                        static_cast<unsigned long>(exceptionCode),
                        static_cast<unsigned long>(exceptionAddress),
                        static_cast<unsigned long>(moduleBase),
                        static_cast<unsigned long>(exceptionRva));
            WriteAll(hLog, text);

            if (pRecord &&
                ((exceptionCode == EXCEPTION_ACCESS_VIOLATION) ||
                 (exceptionCode == EXCEPTION_IN_PAGE_ERROR)) &&
                (pRecord->NumberParameters >= 2))
            {
                const char* operation = "unknown";
                if (pRecord->ExceptionInformation[0] == 0)
                    operation = "read";
                else if (pRecord->ExceptionInformation[0] == 1)
                    operation = "write";
                else if (pRecord->ExceptionInformation[0] == 8)
                    operation = "execute";

                _snprintf_s(text,
                            COUNT_OF(text),
                            _TRUNCATE,
                            "AccessType: %s\r\n"
                            "AccessAddress: 0x%08lX\r\n",
                            operation,
                            static_cast<unsigned long>(pRecord->ExceptionInformation[1]));
                WriteAll(hLog, text);
            };

#if defined(_M_IX86)
            if (pContext)
            {
                _snprintf_s(text,
                            COUNT_OF(text),
                            _TRUNCATE,
                            "EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX\r\n"
                            "ESI=%08lX EDI=%08lX EBP=%08lX ESP=%08lX\r\n"
                            "EIP=%08lX EFLAGS=%08lX\r\n",
                            static_cast<unsigned long>(pContext->Eax),
                            static_cast<unsigned long>(pContext->Ebx),
                            static_cast<unsigned long>(pContext->Ecx),
                            static_cast<unsigned long>(pContext->Edx),
                            static_cast<unsigned long>(pContext->Esi),
                            static_cast<unsigned long>(pContext->Edi),
                            static_cast<unsigned long>(pContext->Ebp),
                            static_cast<unsigned long>(pContext->Esp),
                            static_cast<unsigned long>(pContext->Eip),
                            static_cast<unsigned long>(pContext->EFlags));
                WriteAll(hLog, text);
            };
#endif /* _M_IX86 */

            WriteAll(hLog,
                     "Trace: see TMNT2_Slashuur_trace.log\r\n"
                     "Dump: see the matching TMNT2_crash_*.dmp file\r\n");
            CloseHandle(hLog);
        };

        WriteMiniDump(dumpPath, pException);
        return EXCEPTION_EXECUTE_HANDLER;
    };
}; /* anonymous namespace */


/*static*/ void CPCCrashReporter::Install(void)
{
    char path[MAX_PATH] = {};
    BuildGamePath(path, COUNT_OF(path), "TMNT2_Slashuur_trace.log");

    if (s_hTrace != INVALID_HANDLE_VALUE)
    {
        CloseHandle(s_hTrace);
        s_hTrace = INVALID_HANDLE_VALUE;
    };

    s_hTrace = CreateFileA(path,
                           GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr,
                           CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (s_hTrace != INVALID_HANDLE_VALUE)
    {
        WriteAll(s_hTrace,
                 "TMNT2 Playable Slashuur runtime trace\r\n"
                 "Crash logger: version 2 (buffered trace)\r\n");
    };

    s_handlingCrash = 0;
    s_pPreviousFilter = SetUnhandledExceptionFilter(CrashFilter);
    Breadcrumb("SESSION crash reporter installed");
};


/*static*/ void CPCCrashReporter::Uninstall(void)
{
    Breadcrumb("SESSION normal shutdown");
    if (s_hTrace != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers(s_hTrace);
        CloseHandle(s_hTrace);
        s_hTrace = INVALID_HANDLE_VALUE;
    };
    SetUnhandledExceptionFilter(s_pPreviousFilter);
    s_pPreviousFilter = nullptr;
};


/*static*/ void CPCCrashReporter::Breadcrumb(const char* format, ...)
{
    if (!format)
        return;

    SYSTEMTIME time = {};
    GetLocalTime(&time);

    char message[1536] = {};
    va_list args;
    va_start(args, format);
    _vsnprintf_s(message, COUNT_OF(message), _TRUNCATE, format, args);
    va_end(args);

    char line[2048] = {};
    _snprintf_s(line,
                COUNT_OF(line),
                _TRUNCATE,
                "[%02u:%02u:%02u.%03u][thread %lu] %s\r\n",
                time.wHour,
                time.wMinute,
                time.wSecond,
                time.wMilliseconds,
                static_cast<unsigned long>(GetCurrentThreadId()),
                message);
    AppendTraceText(line);
};


/*static*/ void CPCCrashReporter::FatalMessage(const char* message)
{
    Breadcrumb("FATAL %s", (message ? message : "(null)"));

    char path[MAX_PATH] = {};
    BuildTimestampedGamePath(path, COUNT_OF(path), "TMNT2_fatal", "log");

    HANDLE hFile = CreateFileA(path,
                               GENERIC_WRITE,
                               FILE_SHARE_READ,
                               nullptr,
                               CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        WriteAll(hFile,
                 "TMNT2 Playable Slashuur fatal report\r\n"
                 "Trace: see TMNT2_Slashuur_trace.log\r\n"
                 "Reason:\r\n");
        WriteAll(hFile, (message ? message : "(null)"));
        WriteAll(hFile, "\r\n");
        CloseHandle(hFile);
    };
};
