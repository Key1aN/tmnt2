#include "PCModFeatures.hpp"
#include "PCCrashReporter.hpp"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <cwchar>


unsigned int CPCModFeatures::m_flags = 0;


namespace
{
    bool GetFeatureStatePath(wchar_t* path, size_t capacity)
    {
        DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(capacity));
        if ((length == 0) || (length >= capacity))
            return false;

        wchar_t* separator = std::wcsrchr(path, L'\\');
        if (!separator)
            return false;

        *(separator + 1) = L'\0';
        const wchar_t* relative = L".t2-mod-manager\\features.txt";
        size_t used = std::wcslen(path);
        size_t remaining = capacity - used;
        return (wcscat_s(path, capacity, relative) == 0) &&
               (std::wcslen(relative) < remaining);
    };


    void TrimLine(char* line)
    {
        size_t length = std::strlen(line);
        while (length > 0)
        {
            char value = line[length - 1];
            if ((value != '\r') && (value != '\n') && (value != ' ') && (value != '\t'))
                break;
            line[--length] = '\0';
        };
    };
};


/*static*/ void CPCModFeatures::Initialize(void)
{
    m_flags = 0;

    wchar_t path[MAX_PATH] = {};
    if (!GetFeatureStatePath(path, COUNT_OF(path)))
    {
        CPCCrashReporter::Breadcrumb("T2CORE feature_state path=unavailable flags=0");
        return;
    }

    FILE* stream = nullptr;
    if (_wfopen_s(&stream, path, L"rb") != 0 || !stream)
    {
        CPCCrashReporter::Breadcrumb("T2CORE feature_state file=missing flags=0");
        return;
    }

    char line[160] = {};
    while (std::fgets(line, COUNT_OF(line), stream))
    {
        TrimLine(line);
        if (std::strcmp(line, "feature=intro-skip") == 0)
            m_flags |= FLAG_INTRO_SKIP;
        else if (std::strcmp(line, "feature=msaa") == 0)
            m_flags |= FLAG_MSAA;
        else if (std::strcmp(line, "feature=brutal-difficulties") == 0)
            m_flags |= FLAG_BRUTAL_DIFFICULTIES;
        else if (std::strcmp(line, "feature=widescreen") == 0)
            m_flags |= FLAG_WIDESCREEN;
    }
    std::fclose(stream);

    CPCCrashReporter::Breadcrumb("T2CORE feature_state flags=0x%02X", m_flags);
};


/*static*/ bool CPCModFeatures::IsIntroSkipEnabled(void)
{
    return ((m_flags & FLAG_INTRO_SKIP) != 0);
};


/*static*/ bool CPCModFeatures::IsMSAAEnabled(void)
{
    return ((m_flags & FLAG_MSAA) != 0);
};


/*static*/ bool CPCModFeatures::IsBrutalDifficultiesEnabled(void)
{
    return ((m_flags & FLAG_BRUTAL_DIFFICULTIES) != 0);
};


/*static*/ bool CPCModFeatures::IsWidescreenEnabled(void)
{
    return ((m_flags & FLAG_WIDESCREEN) != 0);
};
