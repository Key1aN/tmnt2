#include "PCFrameStats.hpp"

#if defined(TMNT2_DEBUG_TOOLS)

#include "PCTypedefs.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>


namespace
{
    static CPCFrameStats::SAMPLE s_aSample[CPCFrameStats::HISTORY] = {};
    static uint32 s_uFrameIndex = 0;
    static LARGE_INTEGER s_freq = {};
    static LARGE_INTEGER s_frameBegin = {};
    static LARGE_INTEGER s_updateDone = {};
    static LARGE_INTEGER s_renderDone = {};
    static LARGE_INTEGER s_lastFrameEnd = {};
    static bool s_bDrawSkipped = false;
    static const CPCFrameStats::SAMPLE s_emptySample = {};


    static LARGE_INTEGER Now(void)
    {
        LARGE_INTEGER counter = {};
        QueryPerformanceCounter(&counter);
        return counter;
    };


    static float ElapsedMs(const LARGE_INTEGER& from, const LARGE_INTEGER& to)
    {
        if (s_freq.QuadPart == 0)
            QueryPerformanceFrequency(&s_freq);

        if ((s_freq.QuadPart == 0) || (from.QuadPart == 0) || (to.QuadPart < from.QuadPart))
            return 0.0f;

        double ms = (static_cast<double>(to.QuadPart - from.QuadPart) * 1000.0) /
                    static_cast<double>(s_freq.QuadPart);
        return static_cast<float>(ms);
    };


    static uint64 NowMs(void)
    {
        return static_cast<uint64>(GetTickCount());
    };


    static void BuildGamePath(char* path, size_t pathSize, const char* filename)
    {
        path[0] = '\0';
        DWORD length = GetModuleFileNameA(nullptr, path, static_cast<DWORD>(pathSize));
        if (!length || (length >= pathSize))
        {
            _snprintf_s(path, pathSize, _TRUNCATE, ".\\%s", filename);
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
            path[0] = '\0';

        size_t used = std::strlen(path);
        _snprintf_s(path + used, pathSize - used, _TRUNCATE, "%s", filename);
    };
};


/*static*/ void CPCFrameStats::FrameBegin(void)
{
    s_frameBegin = Now();
    s_bDrawSkipped = false;
};


/*static*/ void CPCFrameStats::UpdateDone(void)
{
    s_updateDone = Now();
};


/*static*/ void CPCFrameStats::SetDrawSkipped(bool bSkipped)
{
    s_bDrawSkipped = bSkipped;
};


/*static*/ void CPCFrameStats::RenderDone(void)
{
    s_renderDone = Now();
};


/*static*/ void CPCFrameStats::FrameEnd(void)
{
    LARGE_INTEGER frameEnd = Now();

    SAMPLE& sample = s_aSample[s_uFrameIndex % HISTORY];
    sample.m_fFrameMs   = (s_lastFrameEnd.QuadPart ? ElapsedMs(s_lastFrameEnd, frameEnd) : ElapsedMs(s_frameBegin, frameEnd));
    sample.m_fUpdateMs  = ElapsedMs(s_frameBegin, s_updateDone);
    sample.m_fRenderMs  = ElapsedMs(s_updateDone, s_renderDone);
    sample.m_fPresentMs = ElapsedMs(s_renderDone, frameEnd);
    sample.m_bDrawn     = !s_bDrawSkipped;
    sample.m_uIndex     = s_uFrameIndex;

    s_lastFrameEnd = frameEnd;
    ++s_uFrameIndex;
};


/*static*/ uint32 CPCFrameStats::GetFrameIndex(void)
{
    return s_uFrameIndex;
};


/*static*/ int32 CPCFrameStats::GetHistoryCount(void)
{
    return (s_uFrameIndex < static_cast<uint32>(HISTORY) ? static_cast<int32>(s_uFrameIndex) : HISTORY);
};


/*static*/ const CPCFrameStats::SAMPLE& CPCFrameStats::GetSample(int32 ago)
{
    if ((ago < 0) || (ago >= GetHistoryCount()))
        return s_emptySample;

    uint32 index = (s_uFrameIndex - 1u - static_cast<uint32>(ago));
    return s_aSample[index % HISTORY];
};


//
// *********************************************************************************
//


CPCDebugLogFile::CPCDebugLogFile(const char* pszFileName)
: m_szFileName()
, m_pFile(nullptr)
, m_bOpenFailed(false)
, m_uLastFlush(0)
{
    _snprintf_s(m_szFileName, sizeof(m_szFileName), _TRUNCATE, "%s", pszFileName);
};


CPCDebugLogFile::~CPCDebugLogFile(void)
{
    if (m_pFile)
    {
        std::fflush(static_cast<FILE*>(m_pFile));
        std::fclose(static_cast<FILE*>(m_pFile));
        m_pFile = nullptr;
    };
};


bool CPCDebugLogFile::Open(void)
{
    if (m_pFile)
        return true;

    if (m_bOpenFailed)
        return false;

    char path[MAX_PATH] = {};
    BuildGamePath(path, sizeof(path), m_szFileName);

    // Keep logs bounded: start a fresh file once it grows past 8 MB.
    const char* pszMode = "ab";
    WIN32_FILE_ATTRIBUTE_DATA attributes = {};
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &attributes) &&
        ((attributes.nFileSizeHigh != 0) || (attributes.nFileSizeLow > (8u * 1024u * 1024u))))
        pszMode = "wb";

    FILE* pFile = nullptr;
    if ((fopen_s(&pFile, path, pszMode) != 0) || !pFile)
    {
        m_bOpenFailed = true;
        return false;
    };

    std::setvbuf(pFile, nullptr, _IOFBF, 64 * 1024);
    m_pFile = pFile;
    m_uLastFlush = NowMs();
    return true;
};


void CPCDebugLogFile::Write(const char* pszFormat, ...)
{
    if (!Open())
        return;

    SYSTEMTIME time = {};
    GetLocalTime(&time);

    char line[1024] = {};
    int32 prefix = _snprintf_s(line,
                               sizeof(line),
                               _TRUNCATE,
                               "%04u-%02u-%02u %02u:%02u:%02u.%03u  ",
                               time.wYear,
                               time.wMonth,
                               time.wDay,
                               time.wHour,
                               time.wMinute,
                               time.wSecond,
                               time.wMilliseconds);
    if (prefix < 0)
        prefix = 0;

    va_list args;
    va_start(args, pszFormat);
    _vsnprintf_s(line + prefix, sizeof(line) - static_cast<size_t>(prefix), _TRUNCATE, pszFormat, args);
    va_end(args);

    FILE* pFile = static_cast<FILE*>(m_pFile);
    std::fputs(line, pFile);
    std::fputs("\r\n", pFile);
};


void CPCDebugLogFile::FlushIfDue(void)
{
    if (!m_pFile)
        return;

    uint64 now = NowMs();
    if ((now < m_uLastFlush) || ((now - m_uLastFlush) >= 500))
        Flush();
};


void CPCDebugLogFile::Flush(void)
{
    if (!m_pFile)
        return;

    std::fflush(static_cast<FILE*>(m_pFile));
    m_uLastFlush = NowMs();
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
