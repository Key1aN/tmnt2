//
//  CombatLab.exe - lightweight launcher for the TMNT2 Combat Laboratory.
//
//  It contains no game code. It starts TMNT2.exe from its own folder with
//  "--combat-lab" plus any arguments given to it (for example -wnd), so the
//  lab always runs the real engine and the same implementation as
//  "TMNT2.exe --combat-lab". Requires a Debug Tools core with the
//  debug-tools-f4 feature enabled; otherwise the game starts normally.
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>


static void ShowError(const wchar_t* pszText)
{
    MessageBoxW(nullptr, pszText, L"TMNT2 Combat Lab", MB_OK | MB_ICONERROR);
};


int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    wchar_t szSelf[MAX_PATH * 4];
    DWORD len = GetModuleFileNameW(nullptr, szSelf, MAX_PATH * 4);
    if ((len == 0) || (len >= (MAX_PATH * 4)))
    {
        ShowError(L"Could not resolve the launcher location.");
        return 1;
    };

    wchar_t* pszSlash = wcsrchr(szSelf, L'\\');
    if (!pszSlash)
    {
        ShowError(L"Unexpected launcher path.");
        return 1;
    };
    pszSlash[1] = L'\0';

    wchar_t szDir[MAX_PATH * 4];
    wcscpy_s(szDir, szSelf);

    wchar_t szGame[MAX_PATH * 4];
    wcscpy_s(szGame, szDir);
    wcscat_s(szGame, L"TMNT2.exe");

    if (GetFileAttributesW(szGame) == INVALID_FILE_ATTRIBUTES)
    {
        ShowError(L"TMNT2.exe was not found next to CombatLab.exe.\n"
                  L"Place CombatLab.exe in the game folder.");
        return 1;
    };

    // Forward everything after our own executable name.
    const wchar_t* pszArgs = GetCommandLineW();
    if (*pszArgs == L'"')
    {
        ++pszArgs;
        while (*pszArgs && (*pszArgs != L'"'))
            ++pszArgs;
        if (*pszArgs == L'"')
            ++pszArgs;
    }
    else
    {
        while (*pszArgs && (*pszArgs != L' ') && (*pszArgs != L'\t'))
            ++pszArgs;
    };
    while ((*pszArgs == L' ') || (*pszArgs == L'\t'))
        ++pszArgs;

    wchar_t szCommand[8192];
    int n = swprintf_s(szCommand, L"\"%s\" --combat-lab%s%s", szGame, (*pszArgs ? L" " : L""), pszArgs);
    if (n < 0)
    {
        ShowError(L"The command line is too long.");
        return 1;
    };

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(szGame, szCommand, nullptr, nullptr, FALSE, 0, nullptr, szDir, &si, &pi))
    {
        ShowError(L"Failed to start TMNT2.exe.");
        return 1;
    };

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
};
