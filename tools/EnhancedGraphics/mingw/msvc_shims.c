/* MSVC CRT pieces referenced by the MSVC-built RenderWare / CRI libraries that msvcrt.dll does not export. */
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int _fltused = 0x9875;
unsigned int __security_cookie = 0xBB40E64Eu;

/* MSVC global operator new / delete (??2@YAPAXI@Z / ??3@YAXPAX@Z, aliased in msvc_shims.S) */
void* __cdecl eg_msvc_operator_new(size_t n) { void* p = malloc(n ? n : 1); if (!p) abort(); return p; }
void __cdecl eg_msvc_operator_delete(void* p) { free(p); }

/* __security_check_cookie failure path */
void __cdecl eg_security_failure(void) { __fastfail(2); }


/* MSVC's _except_handler4 is a thin wrapper over msvcrt.dll's _except_handler4_common (Vista+),
 * which decodes the EH4 scope table with the image's security cookie. Used by __try blocks in CRI. */
void __fastcall __security_check_cookie(unsigned int cookie);
typedef int (__cdecl *EH4COMMON)(unsigned int*, void (__fastcall*)(unsigned int), void*, void*, void*, void*);

int __cdecl _except_handler4(void* pRecord, void* pFrame, void* pContext, void* pDispatcher)
{
    static EH4COMMON s_pfn = 0;
    static int s_bResolved = 0;
    if (!s_bResolved)
    {
        HMODULE hCrt = GetModuleHandleA("msvcrt.dll");
        if (!hCrt)
            hCrt = LoadLibraryA("msvcrt.dll");
        s_pfn = (EH4COMMON)(hCrt ? GetProcAddress(hCrt, "_except_handler4_common") : 0);
        s_bResolved = 1;
    }

    if (!s_pfn)
        return 1;   /* ExceptionContinueSearch: same as having no handler */

    return s_pfn(&__security_cookie, __security_check_cookie, pRecord, pFrame, pContext, pDispatcher);
}
