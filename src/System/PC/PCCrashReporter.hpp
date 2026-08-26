#pragma once


class CPCCrashReporter
{
public:
    static void Install(void);
    static void Uninstall(void);
    static void Breadcrumb(const char* format, ...);
    static void FatalMessage(const char* message);
};
