#include "PCEnhancedGraphics.hpp"
#include "PCTypedefs.hpp"
#include "PCModFeatures.hpp"
#include "PCCrashReporter.hpp"
#include "PCSpecific.hpp"

#include <d3d9.h>

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>

#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>


#if defined(TMNT2_RWDRV_D3D9)
#include "PCEnhancedGraphicsShaders.inl"
#endif /* defined(TMNT2_RWDRV_D3D9) */


extern "C"
{
    extern D3DMATRIX _RwD3D9D3D9ViewTransform;
    extern D3DMATRIX _RwD3D9D3D9ProjTransform;
};


namespace
{
    /*=====================================================================
     *  Presets
     *===================================================================*/

    EGSETTINGS MakeOriginal(void)
    {
        EGSETTINGS s;
        std::memset(&s, 0, sizeof(s));
        s.m_fToonContrast       = 0.0f;
        s.m_fKeyLight           = 1.0f;
        s.m_fAmbientScale       = 1.0f;
        s.m_fShadowOpacity      = 0.62f;
        s.m_fShadowSize         = 1.0f;
        s.m_fContactOpacity     = 0.55f;
        s.m_fWaterStrength      = 1.0f;
        s.m_fWaterWaveScale     = 0.18f;
        s.m_fWaterWaveStrength  = 0.35f;
        s.m_fContrast           = 1.0f;
        s.m_fSaturation         = 1.0f;
        return s;
    };


    EGSETTINGS MakePreset(CEnhancedGraphics::PRESET preset)
    {
        EGSETTINGS s = MakeOriginal();

        if (preset == CEnhancedGraphics::PRESET_CLASSIC_PLUS)
        {
            /* Original lighting, shadows and water; only safer texture filtering. */
            s.m_bTrilinear = true;
            return s;
        };

        s.m_bToon               = true;
        s.m_fToonContrast       = 0.60f;
        s.m_fToonShadowTint     = 0.55f;
        s.m_fToonHighlight      = 0.35f;

        s.m_bLightRig           = true;
        s.m_fKeyLight           = 1.00f;
        s.m_fAmbientScale       = 0.88f;
        s.m_fLightWarmth        = 0.50f;

        s.m_bShadow             = true;
        s.m_fShadowOpacity      = 0.62f;
        s.m_fShadowSize         = 1.10f;
        s.m_bContactShadow      = true;
        s.m_fContactOpacity     = 0.55f;
        s.m_fShadowSunOffset    = 0.12f;

        s.m_bWater              = true;
        s.m_fWaterStrength      = 1.00f;
        s.m_fWaterReflection    = 0.55f;
        s.m_fWaterSpecular      = 0.80f;
        s.m_fWaterFoam          = 0.10f;     /* v1.1.2: most TMNT2 water is calm, indoor or murky */
        s.m_fWaterWaveScale     = 0.18f;
        s.m_fWaterWaveStrength  = 0.20f;
        s.m_bWaterDetail        = false;

        s.m_bBloom              = true;
        s.m_fBloomIntensity     = 0.14f;
        s.m_fBloomThreshold     = 0.80f;
        s.m_bGrade              = true;
        s.m_fContrast           = 1.04f;
        s.m_fSaturation         = 1.06f;
        s.m_fSplitTone          = 0.60f;
        s.m_fVignette           = 0.12f;
        s.m_bFXAA               = false;

        s.m_bTrilinear          = true;

        if (preset == CEnhancedGraphics::PRESET_HIGH)
        {
            s.m_fToonContrast       = 0.75f;
            s.m_fToonShadowTint     = 0.60f;
            s.m_fToonHighlight      = 0.50f;
            s.m_fKeyLight           = 1.04f;
            s.m_fAmbientScale       = 0.85f;
            s.m_fShadowOpacity      = 0.66f;
            s.m_fShadowSize         = 1.12f;
            s.m_fContactOpacity     = 0.60f;
            s.m_fShadowSunOffset    = 0.15f;
            s.m_fWaterReflection    = 0.70f;
            s.m_fWaterSpecular      = 1.00f;
            s.m_fWaterFoam          = 0.25f;
            s.m_bWaterDetail        = true;
            s.m_fBloomIntensity     = 0.17f;
            s.m_fBloomThreshold     = 0.78f;
            s.m_bFXAA               = true;
        };

        return s;
    };


    /*=====================================================================
     *  INI keys
     *===================================================================*/

    enum KEYTYPE { KEY_BOOL, KEY_FLOAT };

    struct KEYINFO
    {
        const char* m_pszName;
        KEYTYPE     m_type;
        size_t      m_offset;
        float       m_fMin;
        float       m_fMax;
    };

#define EGKEY_B(name, member)            { name, KEY_BOOL,  offsetof(EGSETTINGS, member), 0.0f, 1.0f }
#define EGKEY_F(name, member, lo, hi)    { name, KEY_FLOAT, offsetof(EGSETTINGS, member), lo,   hi   }

    const KEYINFO s_aKeyInfo[] =
    {
        EGKEY_B("ToonLighting",      m_bToon),
        EGKEY_F("ToonContrast",      m_fToonContrast,      0.0f, 1.0f),
        EGKEY_F("ToonShadowTint",    m_fToonShadowTint,    0.0f, 1.0f),
        EGKEY_F("ToonHighlight",     m_fToonHighlight,     0.0f, 1.0f),
        EGKEY_B("LightRig",          m_bLightRig),
        EGKEY_F("KeyLight",          m_fKeyLight,          0.5f, 1.5f),
        EGKEY_F("AmbientScale",      m_fAmbientScale,      0.5f, 1.2f),
        EGKEY_F("LightWarmth",       m_fLightWarmth,       0.0f, 1.0f),
        EGKEY_B("Shadows",           m_bShadow),
        EGKEY_F("ShadowOpacity",     m_fShadowOpacity,     0.1f, 1.0f),
        EGKEY_F("ShadowSize",        m_fShadowSize,        0.6f, 1.6f),
        EGKEY_B("ContactShadow",     m_bContactShadow),
        EGKEY_F("ContactOpacity",    m_fContactOpacity,    0.0f, 1.0f),
        EGKEY_F("ShadowSunOffset",   m_fShadowSunOffset,   0.0f, 0.4f),
        EGKEY_B("Water",             m_bWater),
        EGKEY_F("WaterStrength",     m_fWaterStrength,     0.0f, 1.0f),
        EGKEY_F("WaterReflection",   m_fWaterReflection,   0.0f, 1.0f),
        EGKEY_F("WaterSpecular",     m_fWaterSpecular,     0.0f, 1.5f),
        EGKEY_F("WaterFoam",         m_fWaterFoam,         0.0f, 1.0f),
        EGKEY_F("WaterWaveScale",    m_fWaterWaveScale,    0.02f, 1.0f),
        EGKEY_F("WaterWaveStrength", m_fWaterWaveStrength, 0.0f, 1.0f),
        EGKEY_B("WaterDetail",       m_bWaterDetail),
        EGKEY_B("Bloom",             m_bBloom),
        EGKEY_F("BloomIntensity",    m_fBloomIntensity,    0.0f, 0.5f),
        EGKEY_F("BloomThreshold",    m_fBloomThreshold,    0.3f, 1.0f),
        EGKEY_B("ColorGrade",        m_bGrade),
        EGKEY_F("Contrast",          m_fContrast,          0.8f, 1.3f),
        EGKEY_F("Saturation",        m_fSaturation,        0.6f, 1.4f),
        EGKEY_F("SplitTone",         m_fSplitTone,         0.0f, 1.0f),
        EGKEY_F("Vignette",          m_fVignette,          0.0f, 0.5f),
        EGKEY_B("FXAA",              m_bFXAA),
        EGKEY_B("Trilinear",         m_bTrilinear),
    };

#undef EGKEY_B
#undef EGKEY_F


    const int32 OVERRIDE_MAX = COUNT_OF(s_aKeyInfo);
    const int32 NAMELIST_MAX = 16;
    const int32 NAME_MAX     = 32;
    const int32 APPLY_MAX    = 8;


    struct NAMELIST
    {
        int32 m_nCount;
        char  m_aszName[NAMELIST_MAX][NAME_MAX];
    };


    struct STATE
    {
        bool                m_bAvailable;
        bool                m_bEnhanced;
        bool                m_bToastEnabled;
        bool                m_bPerfLog;
        CEnhancedGraphics::PRESET m_preset;

        EGSETTINGS          m_custom;           /* values of the [Overrides] section */
        bool                m_abOverride[OVERRIDE_MAX];
        EGSETTINGS          m_effective;
        EGSETTINGS          m_configured;       /* preset + overrides, shown/edited in the menu */

        /* menu readouts */
        float               m_fLastFps;
        float               m_fLastPostMs;
        float               m_fLastWaterMs;
        bool                m_bHaveLastPerf;
        int32               m_nWaterMaterial;

        NAMELIST            m_waterInclude;
        NAMELIST            m_waterExclude;
        bool                m_bWaterCollisionFallback;

        CEnhancedGraphics::APPLYCALLBACK m_apfnApply[APPLY_MAX];
        int32               m_nApply;

        /* stage */
        float               m_afShadowTint[3];
        RwV3d               m_vLightTravel;
        RwRGBA              m_fogColor;
        bool                m_bFog;

        /* toast */
        char                m_szToast[96];
        LONGLONG            m_toastStart;

        /* timing */
        LONGLONG            m_qpcFreq;
        LONGLONG            m_qpcLast;
        LONGLONG            m_qpcWindowStart;
        int32               m_nFrames;
        double              m_dFrameMsSum;
        double              m_dFrameMsMax;
        double              m_dCpuMsSum;
        double              m_dGpuWaterMsSum;
        double              m_dGpuPostMsSum;
        int32               m_nGpuSamples;

        FILE*               m_pLog;
        wchar_t             m_wszDir[MAX_PATH];
    };


    STATE s_state;


    LONGLONG Qpc(void)
    {
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        return li.QuadPart;
    };


    double QpcToMs(LONGLONG delta)
    {
        if (s_state.m_qpcFreq <= 0)
            return 0.0;
        return (static_cast<double>(delta) * 1000.0) / static_cast<double>(s_state.m_qpcFreq);
    };


    float Clampf(float v, float lo, float hi)
    {
        return (v < lo) ? lo : ((v > hi) ? hi : v);
    };


    float Lerpf(float a, float b, float t)
    {
        return a + (b - a) * t;
    };


    /*=====================================================================
     *  Files
     *===================================================================*/

    bool BuildPath(wchar_t* pwszOut, size_t capacity, const wchar_t* pwszFile)
    {
        if (!s_state.m_wszDir[0])
            return false;

        if ((std::wcslen(s_state.m_wszDir) + std::wcslen(pwszFile) + 1) > capacity)
            return false;

        std::wcscpy(pwszOut, s_state.m_wszDir);
        std::wcscat(pwszOut, pwszFile);
        return true;
    };


    void InitDirectory(void)
    {
        s_state.m_wszDir[0] = L'\0';

        wchar_t wszPath[MAX_PATH] = {};
        DWORD length = GetModuleFileNameW(nullptr, wszPath, MAX_PATH);
        if ((length == 0) || (length >= MAX_PATH))
            return;

        wchar_t* pSeparator = std::wcsrchr(wszPath, L'\\');
        if (!pSeparator)
            return;

        *(pSeparator + 1) = L'\0';
        std::wcscpy(s_state.m_wszDir, wszPath);
    };


    const char* PresetIniName(CEnhancedGraphics::PRESET preset)
    {
        switch (preset)
        {
        case CEnhancedGraphics::PRESET_CLASSIC_PLUS: return "ClassicPlus";
        case CEnhancedGraphics::PRESET_ENHANCED:     return "Enhanced";
        case CEnhancedGraphics::PRESET_HIGH:         return "High";
        case CEnhancedGraphics::PRESET_CUSTOM:       return "Custom";
        default:                                     return "Enhanced";
        };
    };


    void WriteDefaultIni(const wchar_t* pwszPath)
    {
        FILE* pFile = nullptr;
        if ((_wfopen_s(&pFile, pwszPath, L"wb") != 0) || !pFile)
            return;

        static const char* s_apszLine[] =
        {
            "; TMNT2 Enhanced Graphics v1.1 settings",
            "; Read at game start. Ctrl+F8 reloads this file in game.",
            "; F8 = original / enhanced   Shift+F8 = next preset",
            "",
            "[Graphics]",
            "; ClassicPlus | Enhanced | High | Custom",
            "Preset=Enhanced",
            "; 1 = start with the enhancements on, 0 = start on the original look",
            "StartEnhanced=1",
            "; on-screen confirmation after F8 / Shift+F8",
            "Toast=1",
            "; 10-second FPS / GPU-cost summaries in TMNT2_graphics.log",
            "PerfLog=1",
            "",
            "[Overrides]",
            "; Any key below overrides the selected preset. Remove the ';' to use it.",
            ";ToonLighting=1",
            ";ToonContrast=0.60",
            ";ToonShadowTint=0.55",
            ";ToonHighlight=0.35",
            ";LightRig=1",
            ";KeyLight=1.00",
            ";AmbientScale=0.88",
            ";LightWarmth=0.50",
            ";Shadows=1",
            ";ShadowOpacity=0.62",
            ";ShadowSize=1.10",
            ";ContactShadow=1",
            ";ContactOpacity=0.55",
            ";ShadowSunOffset=0.12",
            ";Water=1",
            ";WaterStrength=1.00",
            ";WaterReflection=0.55",
            ";WaterSpecular=0.80",
            ";WaterFoam=0.10",
            ";WaterWaveScale=0.18",
            ";WaterWaveStrength=0.20",
            ";WaterDetail=0",
            ";Bloom=1",
            ";BloomIntensity=0.14",
            ";BloomThreshold=0.80",
            ";ColorGrade=1",
            ";Contrast=1.04",
            ";Saturation=1.06",
            ";SplitTone=0.60",
            ";Vignette=0.12",
            ";FXAA=0",
            ";Trilinear=1",
            "",
            "[Water]",
            "; Texture names (comma separated) to force as water / never treat as water.",
            "; TMNT2_graphics.log lists every candidate material per stage.",
            "Include=",
            "Exclude=",
            "; 1 = when a stage's render polygons are unavailable, draw on collision water polygons",
            "CollisionFallback=0",
        };

        for (int32 i = 0; i < COUNT_OF(s_apszLine); ++i)
        {
            std::fputs(s_apszLine[i], pFile);
            std::fputs("\r\n", pFile);
        };

        std::fclose(pFile);
    };


    void TrimInPlace(char* psz)
    {
        size_t length = std::strlen(psz);
        while ((length > 0) && ((psz[length - 1] == '\r') || (psz[length - 1] == '\n') ||
                                (psz[length - 1] == ' ')  || (psz[length - 1] == '\t')))
        {
            psz[--length] = '\0';
        };

        size_t start = 0;
        while ((psz[start] == ' ') || (psz[start] == '\t'))
            ++start;

        if (start)
            std::memmove(psz, psz + start, std::strlen(psz + start) + 1);
    };


    void ParseNameList(NAMELIST& rList, const char* pszValue)
    {
        rList.m_nCount = 0;

        char szItem[NAME_MAX] = {};
        int32 n = 0;
        for (const char* p = pszValue; ; ++p)
        {
            if ((*p == ',') || (*p == '\0'))
            {
                szItem[n] = '\0';
                TrimInPlace(szItem);
                if (szItem[0] && (rList.m_nCount < NAMELIST_MAX))
                    std::strcpy(rList.m_aszName[rList.m_nCount++], szItem);
                n = 0;
                if (*p == '\0')
                    break;
            }
            else if (n < (NAME_MAX - 1))
            {
                szItem[n++] = *p;
            };
        };
    };


    bool NameListContains(const NAMELIST& rList, const char* pszName)
    {
        if (!pszName)
            return false;

        for (int32 i = 0; i < rList.m_nCount; ++i)
        {
            if (_stricmp(rList.m_aszName[i], pszName) == 0)
                return true;
        };

        return false;
    };


    void LoadIni(void)
    {
        s_state.m_preset = CEnhancedGraphics::PRESET_ENHANCED;
        s_state.m_bEnhanced = true;
        s_state.m_bToastEnabled = true;
        s_state.m_bPerfLog = true;
        s_state.m_custom = MakePreset(CEnhancedGraphics::PRESET_ENHANCED);
        std::memset(s_state.m_abOverride, 0, sizeof(s_state.m_abOverride));
        s_state.m_waterInclude.m_nCount = 0;
        s_state.m_waterExclude.m_nCount = 0;
        s_state.m_bWaterCollisionFallback = false;

        wchar_t wszPath[MAX_PATH] = {};
        if (!BuildPath(wszPath, MAX_PATH, L"TMNT2_graphics.ini"))
            return;

        FILE* pFile = nullptr;
        if ((_wfopen_s(&pFile, wszPath, L"rb") != 0) || !pFile)
        {
            WriteDefaultIni(wszPath);
            CEnhancedGraphics::Log("INI not found: wrote defaults (Preset=Enhanced)");
            return;
        };

        char szSection[32] = {};
        char szLine[512] = {};
        int32 nOverrides = 0;
        while (std::fgets(szLine, sizeof(szLine), pFile))
        {
            TrimInPlace(szLine);
            if (!szLine[0] || (szLine[0] == ';') || (szLine[0] == '#'))
                continue;

            if (szLine[0] == '[')
            {
                char* pEnd = std::strchr(szLine, ']');
                if (pEnd)
                    *pEnd = '\0';
                std::strncpy(szSection, szLine + 1, sizeof(szSection) - 1);
                continue;
            };

            char* pEq = std::strchr(szLine, '=');
            if (!pEq)
                continue;

            *pEq = '\0';
            char* pszKey = szLine;
            char* pszValue = pEq + 1;
            TrimInPlace(pszKey);
            TrimInPlace(pszValue);

            if (_stricmp(szSection, "Graphics") == 0)
            {
                if (_stricmp(pszKey, "Preset") == 0)
                {
                    for (int32 i = 0; i < CEnhancedGraphics::PRESET_NUM; ++i)
                    {
                        if (_stricmp(pszValue, PresetIniName(CEnhancedGraphics::PRESET(i))) == 0)
                            s_state.m_preset = CEnhancedGraphics::PRESET(i);
                    };
                }
                else if (_stricmp(pszKey, "StartEnhanced") == 0)
                {
                    s_state.m_bEnhanced = (std::atoi(pszValue) != 0);
                }
                else if (_stricmp(pszKey, "Toast") == 0)
                {
                    s_state.m_bToastEnabled = (std::atoi(pszValue) != 0);
                }
                else if (_stricmp(pszKey, "PerfLog") == 0)
                {
                    s_state.m_bPerfLog = (std::atoi(pszValue) != 0);
                };
            }
            else if (_stricmp(szSection, "Overrides") == 0)
            {
                for (int32 i = 0; i < COUNT_OF(s_aKeyInfo); ++i)
                {
                    const KEYINFO& rKey = s_aKeyInfo[i];
                    if (_stricmp(pszKey, rKey.m_pszName) != 0)
                        continue;

                    uint8* pBase = reinterpret_cast<uint8*>(&s_state.m_custom);
                    if (rKey.m_type == KEY_BOOL)
                        *reinterpret_cast<bool*>(pBase + rKey.m_offset) = (std::atoi(pszValue) != 0);
                    else
                        *reinterpret_cast<float*>(pBase + rKey.m_offset) =
                            Clampf(static_cast<float>(std::atof(pszValue)), rKey.m_fMin, rKey.m_fMax);

                    s_state.m_abOverride[i] = true;
                    ++nOverrides;
                };
            }
            else if (_stricmp(szSection, "Water") == 0)
            {
                if (_stricmp(pszKey, "Include") == 0)
                    ParseNameList(s_state.m_waterInclude, pszValue);
                else if (_stricmp(pszKey, "Exclude") == 0)
                    ParseNameList(s_state.m_waterExclude, pszValue);
                else if (_stricmp(pszKey, "CollisionFallback") == 0)
                    s_state.m_bWaterCollisionFallback = (std::atoi(pszValue) != 0);
            };
        };

        std::fclose(pFile);

        CEnhancedGraphics::Log("INI loaded: preset=%s start_enhanced=%d toast=%d perflog=%d overrides=%d water_include=%d water_exclude=%d collision_fallback=%d",
                               PresetIniName(s_state.m_preset),
                               (s_state.m_bEnhanced ? 1 : 0),
                               (s_state.m_bToastEnabled ? 1 : 0),
                               (s_state.m_bPerfLog ? 1 : 0),
                               nOverrides,
                               s_state.m_waterInclude.m_nCount,
                               s_state.m_waterExclude.m_nCount,
                               (s_state.m_bWaterCollisionFallback ? 1 : 0));
    };


    /*=====================================================================
     *  Effective settings
     *===================================================================*/

    void ApplyOverrides(EGSETTINGS& rSettings)
    {
        const uint8* pSrc = reinterpret_cast<const uint8*>(&s_state.m_custom);
        uint8* pDst = reinterpret_cast<uint8*>(&rSettings);

        for (int32 i = 0; i < COUNT_OF(s_aKeyInfo); ++i)
        {
            if (!s_state.m_abOverride[i])
                continue;

            const KEYINFO& rKey = s_aKeyInfo[i];
            if (rKey.m_type == KEY_BOOL)
                *reinterpret_cast<bool*>(pDst + rKey.m_offset) = *reinterpret_cast<const bool*>(pSrc + rKey.m_offset);
            else
                *reinterpret_cast<float*>(pDst + rKey.m_offset) = *reinterpret_cast<const float*>(pSrc + rKey.m_offset);
        };
    };


    void UpdateEffective(const char* pszReason)
    {
        CEnhancedGraphics::PRESET basePreset = s_state.m_preset;
        if (basePreset == CEnhancedGraphics::PRESET_CUSTOM)
            basePreset = CEnhancedGraphics::PRESET_ENHANCED;

        /* Overrides apply to Custom always, and to the other presets too. */
        s_state.m_configured = MakePreset(basePreset);
        ApplyOverrides(s_state.m_configured);

        if (!s_state.m_bAvailable || !s_state.m_bEnhanced)
            s_state.m_effective = MakeOriginal();
        else
            s_state.m_effective = s_state.m_configured;

        const EGSETTINGS& e = s_state.m_effective;
        CEnhancedGraphics::Log("APPLY reason=%s mode=%s preset=%s toon=%d light=%d shadow=%d contact=%d water=%d bloom=%d grade=%d fxaa=%d trilinear=%d",
                               pszReason,
                               (s_state.m_bEnhanced ? "ENHANCED" : "ORIGINAL"),
                               PresetIniName(s_state.m_preset),
                               e.m_bToon, e.m_bLightRig, e.m_bShadow, e.m_bContactShadow, e.m_bWater,
                               e.m_bBloom, e.m_bGrade, e.m_bFXAA, e.m_bTrilinear);

        CPCCrashReporter::Breadcrumb("EG apply reason=%s mode=%s preset=%s",
                                     pszReason,
                                     (s_state.m_bEnhanced ? "ENHANCED" : "ORIGINAL"),
                                     PresetIniName(s_state.m_preset));

        for (int32 i = 0; i < s_state.m_nApply; ++i)
            s_state.m_apfnApply[i]();

        /* A mode change starts a fresh performance window so before/after numbers stay separate. */
        s_state.m_qpcWindowStart = 0;
    };


    void ShowToast(void)
    {
        if (!s_state.m_bToastEnabled)
            return;

        if (s_state.m_bEnhanced)
            std::snprintf(s_state.m_szToast, sizeof(s_state.m_szToast), "GRAPHICS: ENHANCED (%s)", CEnhancedGraphics::GetPresetName(s_state.m_preset));
        else
            std::snprintf(s_state.m_szToast, sizeof(s_state.m_szToast), "GRAPHICS: ORIGINAL");

        s_state.m_toastStart = Qpc();
    };


    /*=====================================================================
     *  Stage colours
     *===================================================================*/

    void ComputeShadowTint(void)
    {
        /* Cool shadow hue, nudged toward the stage's fog colour, luminance-normalised (hue only). */
        float cool[3] = { 0.84f, 0.90f, 1.10f };
        float tint[3] = { cool[0], cool[1], cool[2] };

        if (s_state.m_bFog)
        {
            float fog[3] =
            {
                static_cast<float>(s_state.m_fogColor.red)   / 255.0f,
                static_cast<float>(s_state.m_fogColor.green) / 255.0f,
                static_cast<float>(s_state.m_fogColor.blue)  / 255.0f,
            };

            float lum = (fog[0] * 0.299f) + (fog[1] * 0.587f) + (fog[2] * 0.114f);
            if (lum > 0.02f)
            {
                for (int32 i = 0; i < 3; ++i)
                {
                    float hue = Clampf(fog[i] / lum, 0.7f, 1.3f);
                    tint[i] = Lerpf(cool[i], hue, 0.35f);
                };
            };
        };

        float lum = (tint[0] * 0.299f) + (tint[1] * 0.587f) + (tint[2] * 0.114f);
        for (int32 i = 0; i < 3; ++i)
            s_state.m_afShadowTint[i] = tint[i] / lum;
    };


#if defined(TMNT2_RWDRV_D3D9)

    /*=====================================================================
     *  Direct3D 9 resources
     *===================================================================*/

    struct FSVERTEX
    {
        float x, y, z, w;
        float u, v;
    };


    struct GPUQUERYSET
    {
        IDirect3DQuery9* m_pDisjoint;
        IDirect3DQuery9* m_pFreq;
        IDirect3DQuery9* m_apStamp[4];   /* water begin/end, post begin/end */
        bool             m_bIssued;
        bool             m_bWater;
        bool             m_bPost;
    };


    const int32 GPUQUERY_RING = 4;


    struct D3DRES
    {
        bool                            m_bTried;
        bool                            m_bShaderOK;
        bool                            m_bPostBroken;
        bool                            m_bWaterBroken;
        bool                            m_bCallbacksInstalled;

        IDirect3DVertexShader9*         m_pFullscreenVS;
        IDirect3DPixelShader9*          m_pBrightPS;
        IDirect3DPixelShader9*          m_pBlurPS;
        IDirect3DPixelShader9*          m_pCompositePS;
        IDirect3DVertexShader9*         m_pWaterVS;
        IDirect3DPixelShader9*          m_pWaterPS;
        IDirect3DVertexDeclaration9*    m_pFullscreenDecl;
        IDirect3DVertexDeclaration9*    m_pWaterDecl;

        /* D3DPOOL_MANAGED: survive device reset */
        IDirect3DTexture9*              m_pWaveTex;
        IDirect3DVertexBuffer9*         m_pWaterVB;
        IDirect3DIndexBuffer9*          m_pWaterIB;
        int32                           m_nWaterVertex;
        int32                           m_nWaterIndex;
        float                           m_afWaterBase[3];
        float                           m_fWaterTime;

        /* D3DPOOL_DEFAULT: released on device loss, recreated on demand */
        IDirect3DTexture9*              m_pSceneTex;
        IDirect3DTexture9*              m_pBloomTexA;
        IDirect3DTexture9*              m_pBloomTexB;
        UINT                            m_uRTWidth;
        UINT                            m_uRTHeight;
        D3DFORMAT                       m_rtFormat;

        GPUQUERYSET                     m_aQuery[GPUQUERY_RING];
        int32                           m_iQuery;
        bool                            m_bQueryBroken;
        int32                           m_iActiveQuery;

        rwD3D9DeviceReleaseCallBack     m_pfnPrevRelease;
        rwD3D9DeviceRestoreCallBack     m_pfnPrevRestore;
    };


    D3DRES s_d3d;


    template<class T>
    void SafeRelease(T*& rp)
    {
        if (rp)
        {
            rp->Release();
            rp = nullptr;
        };
    };


    IDirect3DDevice9* Device(void)
    {
        return static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
    };


    void ReleaseDefaultPool(void)
    {
        SafeRelease(s_d3d.m_pSceneTex);
        SafeRelease(s_d3d.m_pBloomTexA);
        SafeRelease(s_d3d.m_pBloomTexB);
        s_d3d.m_uRTWidth = 0;
        s_d3d.m_uRTHeight = 0;

        for (int32 i = 0; i < GPUQUERY_RING; ++i)
        {
            GPUQUERYSET& q = s_d3d.m_aQuery[i];
            SafeRelease(q.m_pDisjoint);
            SafeRelease(q.m_pFreq);
            for (int32 j = 0; j < COUNT_OF(q.m_apStamp); ++j)
                SafeRelease(q.m_apStamp[j]);
            q.m_bIssued = false;
        };
        s_d3d.m_iActiveQuery = -1;
    };


    void DeviceReleaseCallback(void)
    {
        if (s_d3d.m_pfnPrevRelease)
            s_d3d.m_pfnPrevRelease();

        ReleaseDefaultPool();
        CEnhancedGraphics::Log("DEVICE release: default-pool render targets and queries released");
    };


    void DeviceRestoreCallback(void)
    {
        if (s_d3d.m_pfnPrevRestore)
            s_d3d.m_pfnPrevRestore();

        /* Render targets are recreated on the next post-processed frame. */
        CEnhancedGraphics::Log("DEVICE restore");
    };


    void BuildWaveTexture(IDirect3DDevice9* pDevice)
    {
        const UINT SIZE = 128;
        IDirect3DTexture9* pTex = nullptr;
        if (FAILED(pDevice->CreateTexture(SIZE, SIZE, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTex, nullptr)) || !pTex)
        {
            CEnhancedGraphics::Log("WATER wave texture create failed");
            return;
        };

        /* Tileable height field: sum of integer-frequency waves (fixed seed, identical every run). */
        static float s_afHeight[SIZE * SIZE];
        static float s_afFoam[SIZE * SIZE];
        struct WAVE { float fx, fy, amp, phase; };
        WAVE aWave[10];
        WAVE aFoam[6];
        uint32 seed = 0x7A3D91u;
        auto rnd = [&seed](void) -> float
        {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;
        };

        for (int32 i = 0; i < COUNT_OF(aWave); ++i)
        {
            aWave[i].fx    = static_cast<float>(static_cast<int32>(rnd() * 6.0f) + 1) * ((rnd() < 0.5f) ? -1.0f : 1.0f);
            aWave[i].fy    = static_cast<float>(static_cast<int32>(rnd() * 6.0f) + 1);
            aWave[i].amp   = 1.0f / (1.0f + 0.35f * static_cast<float>(i));
            aWave[i].phase = rnd() * 6.2831853f;
        };

        for (int32 i = 0; i < COUNT_OF(aFoam); ++i)
        {
            aFoam[i].fx    = static_cast<float>(static_cast<int32>(rnd() * 10.0f) + 3);
            aFoam[i].fy    = static_cast<float>(static_cast<int32>(rnd() * 10.0f) + 3) * ((rnd() < 0.5f) ? -1.0f : 1.0f);
            aFoam[i].amp   = 1.0f;
            aFoam[i].phase = rnd() * 6.2831853f;
        };

        float fMinH = 1e9f, fMaxH = -1e9f, fMinF = 1e9f, fMaxF = -1e9f;
        for (UINT y = 0; y < SIZE; ++y)
        {
            for (UINT x = 0; x < SIZE; ++x)
            {
                float u = static_cast<float>(x) / static_cast<float>(SIZE);
                float v = static_cast<float>(y) / static_cast<float>(SIZE);
                float h = 0.0f;
                for (int32 i = 0; i < COUNT_OF(aWave); ++i)
                    h += aWave[i].amp * std::sin(6.2831853f * (aWave[i].fx * u + aWave[i].fy * v) + aWave[i].phase);

                float f = 0.0f;
                for (int32 i = 0; i < COUNT_OF(aFoam); ++i)
                    f += std::sin(6.2831853f * (aFoam[i].fx * u + aFoam[i].fy * v) + aFoam[i].phase);

                s_afHeight[y * SIZE + x] = h;
                s_afFoam[y * SIZE + x] = f;
                fMinH = (h < fMinH) ? h : fMinH; fMaxH = (h > fMaxH) ? h : fMaxH;
                fMinF = (f < fMinF) ? f : fMinF; fMaxF = (f > fMaxF) ? f : fMaxF;
            };
        };

        const float fSlope = 2.5f / (fMaxH - fMinH);
        DWORD dwLevels = pTex->GetLevelCount();
        for (DWORD level = 0; level < dwLevels; ++level)
        {
            D3DLOCKED_RECT lr;
            if (FAILED(pTex->LockRect(level, &lr, nullptr, 0)))
                continue;

            UINT size = SIZE >> level;
            UINT step = SIZE / size;
            for (UINT y = 0; y < size; ++y)
            {
                uint32* pRow = reinterpret_cast<uint32*>(static_cast<uint8*>(lr.pBits) + y * lr.Pitch);
                for (UINT x = 0; x < size; ++x)
                {
                    /* box-filter the base level for each mip */
                    float nx = 0.0f, nz = 0.0f, fo = 0.0f;
                    for (UINT sy = 0; sy < step; ++sy)
                    {
                        for (UINT sx = 0; sx < step; ++sx)
                        {
                            UINT bx = x * step + sx;
                            UINT by = y * step + sy;
                            float hL = s_afHeight[by * SIZE + ((bx + SIZE - 1) % SIZE)];
                            float hR = s_afHeight[by * SIZE + ((bx + 1) % SIZE)];
                            float hD = s_afHeight[((by + SIZE - 1) % SIZE) * SIZE + bx];
                            float hU = s_afHeight[((by + 1) % SIZE) * SIZE + bx];
                            nx += -(hR - hL) * fSlope;
                            nz += -(hU - hD) * fSlope;
                            fo += (s_afFoam[by * SIZE + bx] - fMinF) / (fMaxF - fMinF);
                        };
                    };

                    float inv = 1.0f / static_cast<float>(step * step);
                    nx *= inv; nz *= inv; fo *= inv;
                    float ny = 1.0f;
                    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                    nx /= len; ny /= len; nz /= len;

                    uint32 r = static_cast<uint32>(Clampf(nx * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
                    uint32 g = static_cast<uint32>(Clampf(nz * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
                    uint32 b = static_cast<uint32>(Clampf(ny, 0.0f, 1.0f) * 255.0f + 0.5f);
                    uint32 a = static_cast<uint32>(Clampf(fo, 0.0f, 1.0f) * 255.0f + 0.5f);
                    pRow[x] = (a << 24) | (r << 16) | (g << 8) | b;
                };
            };

            pTex->UnlockRect(level);
        };

        s_d3d.m_pWaveTex = pTex;
    };


    void CreateStaticResources(void)
    {
        if (s_d3d.m_bTried)
            return;

        s_d3d.m_bTried = true;

        IDirect3DDevice9* pDevice = Device();
        if (!pDevice)
        {
            CEnhancedGraphics::Log("D3D9 device unavailable: water and post disabled");
            s_d3d.m_bPostBroken = s_d3d.m_bWaterBroken = true;
            return;
        };

        const D3DCAPS9* pCaps = static_cast<const D3DCAPS9*>(RwD3D9GetCaps());
        DWORD vs = (pCaps ? pCaps->VertexShaderVersion : 0);
        DWORD ps = (pCaps ? pCaps->PixelShaderVersion : 0);
        CEnhancedGraphics::Log("D3D9 caps vs=%lu.%lu ps=%lu.%lu stretchrect_filter=0x%08lX max_texture=%lux%lu",
                               (vs >> 8) & 0xFF, vs & 0xFF, (ps >> 8) & 0xFF, ps & 0xFF,
                               (pCaps ? pCaps->StretchRectFilterCaps : 0),
                               (pCaps ? pCaps->MaxTextureWidth : 0),
                               (pCaps ? pCaps->MaxTextureHeight : 0));

        if ((vs < D3DVS_VERSION(3, 0)) || (ps < D3DPS_VERSION(3, 0)))
        {
            CEnhancedGraphics::Log("Shader model 3.0 not supported: water and post disabled (toon, light and shadow changes still work)");
            s_d3d.m_bPostBroken = s_d3d.m_bWaterBroken = true;
            return;
        };

        bool bOK = true;
        bOK &= SUCCEEDED(pDevice->CreateVertexShader(g_EgFullscreenVS, &s_d3d.m_pFullscreenVS));
        bOK &= SUCCEEDED(pDevice->CreatePixelShader(g_EgBrightPS,      &s_d3d.m_pBrightPS));
        bOK &= SUCCEEDED(pDevice->CreatePixelShader(g_EgBlurPS,        &s_d3d.m_pBlurPS));
        bOK &= SUCCEEDED(pDevice->CreatePixelShader(g_EgCompositePS,   &s_d3d.m_pCompositePS));
        if (!bOK)
        {
            CEnhancedGraphics::Log("Post shader creation failed: post disabled");
            s_d3d.m_bPostBroken = true;
        };

        bool bWaterOK = true;
        bWaterOK &= SUCCEEDED(pDevice->CreateVertexShader(g_EgWaterVS, &s_d3d.m_pWaterVS));
        bWaterOK &= SUCCEEDED(pDevice->CreatePixelShader(g_EgWaterPS,  &s_d3d.m_pWaterPS));
        if (!bWaterOK)
        {
            CEnhancedGraphics::Log("Water shader creation failed: water disabled");
            s_d3d.m_bWaterBroken = true;
        };

        static const D3DVERTEXELEMENT9 s_aFullscreenDecl[] =
        {
            { 0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        static const D3DVERTEXELEMENT9 s_aWaterDecl[] =
        {
            { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        if (FAILED(pDevice->CreateVertexDeclaration(s_aFullscreenDecl, &s_d3d.m_pFullscreenDecl)))
            s_d3d.m_bPostBroken = true;

        if (FAILED(pDevice->CreateVertexDeclaration(s_aWaterDecl, &s_d3d.m_pWaterDecl)))
            s_d3d.m_bWaterBroken = true;

        BuildWaveTexture(pDevice);
        if (!s_d3d.m_pWaveTex)
            s_d3d.m_bWaterBroken = true;

        s_d3d.m_bShaderOK = !s_d3d.m_bPostBroken || !s_d3d.m_bWaterBroken;
        CEnhancedGraphics::Log("D3D9 resources post=%s water=%s",
                               (s_d3d.m_bPostBroken ? "DISABLED" : "ready"),
                               (s_d3d.m_bWaterBroken ? "DISABLED" : "ready"));
    };


    void ReleaseStaticResources(void)
    {
        if (s_d3d.m_pWaterVB || s_d3d.m_pWaterIB)
        {
            RwD3D9SetStreamSource(0, nullptr, 0, 0);
            RwD3D9SetIndices(nullptr);
        };

        SafeRelease(s_d3d.m_pWaterVB);
        SafeRelease(s_d3d.m_pWaterIB);
        SafeRelease(s_d3d.m_pWaveTex);
        SafeRelease(s_d3d.m_pFullscreenDecl);
        SafeRelease(s_d3d.m_pWaterDecl);
        SafeRelease(s_d3d.m_pFullscreenVS);
        SafeRelease(s_d3d.m_pBrightPS);
        SafeRelease(s_d3d.m_pBlurPS);
        SafeRelease(s_d3d.m_pCompositePS);
        SafeRelease(s_d3d.m_pWaterVS);
        SafeRelease(s_d3d.m_pWaterPS);
        s_d3d.m_nWaterVertex = 0;
        s_d3d.m_nWaterIndex = 0;
        s_d3d.m_bTried = false;
    };


    /*=====================================================================
     *  State save / restore through RenderWare's cache
     *===================================================================*/

    struct RSSAVE
    {
        RwUInt32 m_state;
        RwUInt32 m_value;
    };


    /* RenderWare's state cache starts every entry at 0xFFFFFFFF ("never set") and only fills in
     * the states RW itself uses. Restoring such an entry wrote 0xFFFFFFFF to the device, e.g.
     * D3DRS_SRGBWRITEENABLE = TRUE, which gamma-encoded every later draw (washed-out, very
     * white frame on real GPUs; wined3d/llvmpipe ignores sRGB writes, so tests missed it).
     * Unknown entries are restored to the Direct3D 9 default instead. */
    static const RwUInt32 RW_STATE_UNKNOWN = 0xFFFFFFFF;

    RwUInt32 D3DDefaultRenderState(RwUInt32 state)
    {
        switch (state)
        {
        case D3DRS_ZENABLE:             return D3DZB_TRUE;
        case D3DRS_ZWRITEENABLE:        return TRUE;
        case D3DRS_ZFUNC:               return D3DCMP_LESSEQUAL;
        case D3DRS_SRCBLEND:            return D3DBLEND_ONE;
        case D3DRS_DESTBLEND:           return D3DBLEND_ZERO;
        case D3DRS_CULLMODE:            return D3DCULL_CCW;
        case D3DRS_COLORWRITEENABLE:    return 0x0000000F;
        default:                        return FALSE;   /* every boolean enable guarded here */
        };
    };


    RwUInt32 D3DDefaultSamplerState(RwUInt32 type)
    {
        switch (type)
        {
        case D3DSAMP_ADDRESSU:
        case D3DSAMP_ADDRESSV:          return D3DTADDRESS_WRAP;
        case D3DSAMP_MINFILTER:
        case D3DSAMP_MAGFILTER:         return D3DTEXF_POINT;
        default:                        return 0;       /* MIPFILTER NONE, SRGBTEXTURE FALSE */
        };
    };


    struct STATEGUARD
    {
        RSSAVE   m_aRS[16];
        int32    m_nRS;
        RwUInt32 m_aSamp[2][6];

        STATEGUARD(void) : m_nRS(0) {};

        void Set(RwUInt32 state, RwUInt32 value)
        {
            ASSERT(m_nRS < COUNT_OF(m_aRS));
            RSSAVE& r = m_aRS[m_nRS++];
            r.m_state = state;
            r.m_value = 0;
            RwD3D9GetRenderState(state, &r.m_value);
            if (r.m_value == RW_STATE_UNKNOWN)
                r.m_value = D3DDefaultRenderState(state);
            RwD3D9SetRenderState(state, value);
        };

        void SaveSamplers(void)
        {
            static const RwUInt32 s_aType[6] =
            {
                D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_MINFILTER,
                D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE,
            };

            for (RwUInt32 stage = 0; stage < 2; ++stage)
            {
                for (int32 i = 0; i < 6; ++i)
                {
                    m_aSamp[stage][i] = 0;
                    RwD3D9GetSamplerState(stage, s_aType[i], &m_aSamp[stage][i]);
                    if (m_aSamp[stage][i] == RW_STATE_UNKNOWN)
                        m_aSamp[stage][i] = D3DDefaultSamplerState(s_aType[i]);
                };
            };
        };

        void RestoreSamplers(void)
        {
            static const RwUInt32 s_aType[6] =
            {
                D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_MINFILTER,
                D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE,
            };

            for (RwUInt32 stage = 0; stage < 2; ++stage)
            {
                for (int32 i = 0; i < 6; ++i)
                    RwD3D9SetSamplerState(stage, s_aType[i], m_aSamp[stage][i]);
            };
        };

        void Restore(void)
        {
            while (m_nRS > 0)
            {
                const RSSAVE& r = m_aRS[--m_nRS];
                RwD3D9SetRenderState(r.m_state, r.m_value);
            };
        };
    };


    void SetSampler(RwUInt32 stage, RwUInt32 address, RwUInt32 filter, RwUInt32 mip)
    {
        RwD3D9SetSamplerState(stage, D3DSAMP_ADDRESSU, address);
        RwD3D9SetSamplerState(stage, D3DSAMP_ADDRESSV, address);
        RwD3D9SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
        RwD3D9SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
        RwD3D9SetSamplerState(stage, D3DSAMP_MIPFILTER, mip);
        RwD3D9SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
    };


    /*=====================================================================
     *  GPU timing (timestamp queries, read back a few frames later, never stalls)
     *===================================================================*/

    void QueryCollect(void)
    {
        if (s_d3d.m_bQueryBroken)
            return;

        for (int32 i = 0; i < GPUQUERY_RING; ++i)
        {
            GPUQUERYSET& q = s_d3d.m_aQuery[i];
            if (!q.m_bIssued || (i == s_d3d.m_iActiveQuery))
                continue;

            BOOL bDisjoint = TRUE;
            UINT64 freq = 0;
            if ((q.m_pDisjoint->GetData(&bDisjoint, sizeof(bDisjoint), 0) != S_OK) ||
                (q.m_pFreq->GetData(&freq, sizeof(freq), 0) != S_OK))
                continue;

            UINT64 aStamp[4] = {};
            bool bReady = true;
            for (int32 j = 0; j < 4; ++j)
            {
                bool bUsed = ((j < 2) ? q.m_bWater : q.m_bPost);
                if (bUsed && (q.m_apStamp[j]->GetData(&aStamp[j], sizeof(UINT64), 0) != S_OK))
                    bReady = false;
            };

            if (!bReady)
                continue;

            q.m_bIssued = false;
            if (bDisjoint || (freq == 0))
                continue;

            double dWater = (q.m_bWater ? (static_cast<double>(aStamp[1] - aStamp[0]) * 1000.0 / static_cast<double>(freq)) : 0.0);
            double dPost  = (q.m_bPost  ? (static_cast<double>(aStamp[3] - aStamp[2]) * 1000.0 / static_cast<double>(freq)) : 0.0);
            s_state.m_dGpuWaterMsSum += dWater;
            s_state.m_dGpuPostMsSum += dPost;
            s_state.m_fLastWaterMs += (static_cast<float>(dWater) - s_state.m_fLastWaterMs) * 0.1f;
            s_state.m_fLastPostMs += (static_cast<float>(dPost) - s_state.m_fLastPostMs) * 0.1f;
            ++s_state.m_nGpuSamples;
        };
    };


    void QueryFrameBegin(void)
    {
        s_d3d.m_iActiveQuery = -1;

        if (s_d3d.m_bQueryBroken || !s_state.m_bEnhanced)
            return;

        IDirect3DDevice9* pDevice = Device();
        if (!pDevice)
            return;

        QueryCollect();

        int32 iSlot = s_d3d.m_iQuery;
        GPUQUERYSET& q = s_d3d.m_aQuery[iSlot];
        if (q.m_bIssued)
            return; /* results not back yet: skip timing this frame */

        if (!q.m_pDisjoint)
        {
            bool bOK = SUCCEEDED(pDevice->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &q.m_pDisjoint)) &&
                       SUCCEEDED(pDevice->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &q.m_pFreq));
            for (int32 j = 0; bOK && (j < 4); ++j)
                bOK = SUCCEEDED(pDevice->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &q.m_apStamp[j]));

            if (!bOK)
            {
                CEnhancedGraphics::Log("GPU timestamp queries unsupported: GPU timing disabled (FPS still logged)");
                s_d3d.m_bQueryBroken = true;
                return;
            };
        };

        q.m_bWater = false;
        q.m_bPost = false;
        q.m_pDisjoint->Issue(D3DISSUE_BEGIN);
        s_d3d.m_iActiveQuery = iSlot;
    };


    void QueryStamp(int32 index)
    {
        if (s_d3d.m_iActiveQuery < 0)
            return;

        GPUQUERYSET& q = s_d3d.m_aQuery[s_d3d.m_iActiveQuery];
        q.m_apStamp[index]->Issue(D3DISSUE_END);
        if (index < 2)
            q.m_bWater = true;
        else
            q.m_bPost = true;
    };


    void QueryFrameEnd(void)
    {
        if (s_d3d.m_iActiveQuery < 0)
            return;

        GPUQUERYSET& q = s_d3d.m_aQuery[s_d3d.m_iActiveQuery];
        q.m_pDisjoint->Issue(D3DISSUE_END);
        q.m_pFreq->Issue(D3DISSUE_END);
        q.m_bIssued = true;
        s_d3d.m_iActiveQuery = -1;
        s_d3d.m_iQuery = (s_d3d.m_iQuery + 1) % GPUQUERY_RING;
    };


    /*=====================================================================
     *  Post render targets
     *===================================================================*/

    bool EnsureRenderTargets(IDirect3DDevice9* pDevice, UINT w, UINT h, D3DFORMAT fmt)
    {
        if (s_d3d.m_pSceneTex && (s_d3d.m_uRTWidth == w) && (s_d3d.m_uRTHeight == h) && (s_d3d.m_rtFormat == fmt))
            return true;

        ReleaseDefaultPool();

        UINT qw = (w / 4) ? (w / 4) : 1;
        UINT qh = (h / 4) ? (h / 4) : 1;

        bool bOK =
            SUCCEEDED(pDevice->CreateTexture(w,  h,  1, D3DUSAGE_RENDERTARGET, fmt,              D3DPOOL_DEFAULT, &s_d3d.m_pSceneTex,  nullptr)) &&
            SUCCEEDED(pDevice->CreateTexture(qw, qh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s_d3d.m_pBloomTexA, nullptr)) &&
            SUCCEEDED(pDevice->CreateTexture(qw, qh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &s_d3d.m_pBloomTexB, nullptr));

        if (!bOK)
        {
            ReleaseDefaultPool();
            CEnhancedGraphics::Log("POST render target creation failed for %ux%u fmt=%d: post disabled", w, h, static_cast<int>(fmt));
            s_d3d.m_bPostBroken = true;
            return false;
        };

        s_d3d.m_uRTWidth = w;
        s_d3d.m_uRTHeight = h;
        s_d3d.m_rtFormat = fmt;
        CEnhancedGraphics::Log("POST render targets %ux%u (bloom %ux%u) fmt=%d", w, h, qw, qh, static_cast<int>(fmt));
        return true;
    };


    void DrawFullscreen(IDirect3DDevice9* pDevice, IDirect3DSurface9* pTarget, UINT w, UINT h)
    {
        pDevice->SetRenderTarget(0, pTarget);

        D3DVIEWPORT9 vp = { 0, 0, w, h, 0.0f, 1.0f };
        pDevice->SetViewport(&vp);

        const float hx = 1.0f / static_cast<float>(w);
        const float hy = 1.0f / static_cast<float>(h);
        FSVERTEX aVertex[4] =
        {
            { -1.0f - hx,  1.0f + hy, 0.0f, 1.0f, 0.0f, 0.0f },
            {  1.0f - hx,  1.0f + hy, 0.0f, 1.0f, 1.0f, 0.0f },
            { -1.0f - hx, -1.0f + hy, 0.0f, 1.0f, 0.0f, 1.0f },
            {  1.0f - hx, -1.0f + hy, 0.0f, 1.0f, 1.0f, 1.0f },
        };

        _rwD3D9DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, aVertex, sizeof(FSVERTEX));
    };

#endif /* defined(TMNT2_RWDRV_D3D9) */
};


/*=========================================================================
 *  Public API
 *=======================================================================*/

/*static*/ void CEnhancedGraphics::Initialize(void)
{
    std::memset(&s_state, 0, sizeof(s_state));
#if defined(TMNT2_RWDRV_D3D9)
    std::memset(&s_d3d, 0, sizeof(s_d3d));
    s_d3d.m_iActiveQuery = -1;
#endif /* defined(TMNT2_RWDRV_D3D9) */

    s_state.m_bAvailable = CPCModFeatures::IsEnhancedGraphicsEnabled();
    s_state.m_effective = MakeOriginal();
    s_state.m_afShadowTint[0] = s_state.m_afShadowTint[1] = s_state.m_afShadowTint[2] = 1.0f;
    s_state.m_vLightTravel.x = 0.5f;
    s_state.m_vLightTravel.y = -0.7071f;
    s_state.m_vLightTravel.z = 0.5f;

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    s_state.m_qpcFreq = freq.QuadPart;

    if (!s_state.m_bAvailable)
    {
        CPCCrashReporter::Breadcrumb("EG feature=off");
        return;
    };

    InitDirectory();

    wchar_t wszLog[MAX_PATH] = {};
    if (BuildPath(wszLog, MAX_PATH, L"TMNT2_graphics.log"))
        _wfopen_s(&s_state.m_pLog, wszLog, L"wb");

    Log("TMNT2 Enhanced Graphics v1.1.2 (feature enhanced-graphics)");
    LoadIni();
    ComputeShadowTint();
    UpdateEffective("startup");
    CPCCrashReporter::Breadcrumb("EG feature=on mode=%s preset=%s",
                                 (s_state.m_bEnhanced ? "ENHANCED" : "ORIGINAL"),
                                 PresetIniName(s_state.m_preset));
};


/*static*/ void CEnhancedGraphics::Terminate(void)
{
    if (s_state.m_pLog)
    {
        Log("shutdown");
        std::fclose(s_state.m_pLog);
        s_state.m_pLog = nullptr;
    };
};


/*static*/ void CEnhancedGraphics::OnEngineStarted(void)
{
#if defined(TMNT2_RWDRV_D3D9)
    if (!s_state.m_bAvailable || s_d3d.m_bCallbacksInstalled)
        return;

    s_d3d.m_pfnPrevRelease = _rwD3D9DeviceGetReleaseCallback();
    s_d3d.m_pfnPrevRestore = _rwD3D9DeviceGetRestoreCallback();
    _rwD3D9DeviceSetReleaseCallback(DeviceReleaseCallback);
    _rwD3D9DeviceSetRestoreCallback(DeviceRestoreCallback);
    s_d3d.m_bCallbacksInstalled = true;

    CreateStaticResources();
#endif /* defined(TMNT2_RWDRV_D3D9) */
};


/*static*/ void CEnhancedGraphics::OnEngineStopping(void)
{
#if defined(TMNT2_RWDRV_D3D9)
    if (!s_state.m_bAvailable)
        return;

    ReleaseDefaultPool();
    ReleaseStaticResources();

    if (s_d3d.m_bCallbacksInstalled)
    {
        _rwD3D9DeviceSetReleaseCallback(s_d3d.m_pfnPrevRelease);
        _rwD3D9DeviceSetRestoreCallback(s_d3d.m_pfnPrevRestore);
        s_d3d.m_bCallbacksInstalled = false;
    };

    Log("engine stop: GPU resources released");
#endif /* defined(TMNT2_RWDRV_D3D9) */
};


/*static*/ void CEnhancedGraphics::FrameBegin(void)
{
    if (!s_state.m_bAvailable)
        return;

    LONGLONG now = Qpc();

    /* ---- hotkeys ---- */
    if (CPCSpecific::IsKeyTrigger(DIK_F8))
    {
        bool bShift = CPCSpecific::IsKeyDown(DIK_LSHIFT) || CPCSpecific::IsKeyDown(DIK_RSHIFT);
        bool bCtrl  = CPCSpecific::IsKeyDown(DIK_LCONTROL) || CPCSpecific::IsKeyDown(DIK_RCONTROL);

        if (bCtrl)
        {
            bool bEnhanced = s_state.m_bEnhanced;
            LoadIni();
            s_state.m_bEnhanced = bEnhanced; /* reload keeps the current A/B side */
            UpdateEffective("reload");
        }
        else if (bShift)
        {
            s_state.m_preset = PRESET((s_state.m_preset + 1) % PRESET_NUM);
            s_state.m_bEnhanced = true;
            UpdateEffective("preset");
        }
        else
        {
            s_state.m_bEnhanced = !s_state.m_bEnhanced;
            UpdateEffective("toggle");
        };

        ShowToast();
    };

    /* ---- frame timing ---- */
    if (s_state.m_qpcLast)
    {
        double dMs = QpcToMs(now - s_state.m_qpcLast);
        if (s_state.m_qpcWindowStart == 0)
        {
            s_state.m_qpcWindowStart = now;
            s_state.m_nFrames = 0;
            s_state.m_dFrameMsSum = 0.0;
            s_state.m_dFrameMsMax = 0.0;
            s_state.m_dCpuMsSum = 0.0;
            s_state.m_dGpuWaterMsSum = 0.0;
            s_state.m_dGpuPostMsSum = 0.0;
            s_state.m_nGpuSamples = 0;
        }
        else if (dMs < 1000.0)
        {
            /* live readout for the Debug Tools tab (smoothed, independent of the log window) */
            if (dMs > 0.01)
            {
                float fFps = static_cast<float>(1000.0 / dMs);
                s_state.m_fLastFps = (s_state.m_fLastFps > 0.0f) ? (s_state.m_fLastFps + (fFps - s_state.m_fLastFps) * 0.05f) : fFps;
                s_state.m_bHaveLastPerf = true;
            };

            ++s_state.m_nFrames;
            s_state.m_dFrameMsSum += dMs;
            if (dMs > s_state.m_dFrameMsMax)
                s_state.m_dFrameMsMax = dMs;
        };

        if (s_state.m_bPerfLog && s_state.m_qpcWindowStart && (QpcToMs(now - s_state.m_qpcWindowStart) >= 10000.0) && (s_state.m_nFrames > 0))
        {
            double dAvg = s_state.m_dFrameMsSum / static_cast<double>(s_state.m_nFrames);
            double dGpuW = (s_state.m_nGpuSamples ? (s_state.m_dGpuWaterMsSum / s_state.m_nGpuSamples) : -1.0);
            double dGpuP = (s_state.m_nGpuSamples ? (s_state.m_dGpuPostMsSum / s_state.m_nGpuSamples) : -1.0);
            Log("PERF mode=%s preset=%s frames=%d fps=%.1f avg_ms=%.2f max_ms=%.2f eg_cpu_ms=%.3f gpu_water_ms=%.3f gpu_post_ms=%.3f gpu_samples=%d",
                (s_state.m_bEnhanced ? "ENHANCED" : "ORIGINAL"),
                PresetIniName(s_state.m_preset),
                s_state.m_nFrames,
                1000.0 / dAvg,
                dAvg,
                s_state.m_dFrameMsMax,
                s_state.m_dCpuMsSum / static_cast<double>(s_state.m_nFrames),
                dGpuW,
                dGpuP,
                s_state.m_nGpuSamples);
            s_state.m_qpcWindowStart = 0;
        };
    };
    s_state.m_qpcLast = now;

#if defined(TMNT2_RWDRV_D3D9)
    QueryFrameEnd();    /* closes the previous frame's set, if one is still open */
    QueryFrameBegin();
#endif /* defined(TMNT2_RWDRV_D3D9) */

    s_state.m_dCpuMsSum += QpcToMs(Qpc() - now);
};


/*static*/ bool CEnhancedGraphics::IsAvailable(void)
{
    return s_state.m_bAvailable;
};


/*static*/ bool CEnhancedGraphics::IsEnhanced(void)
{
    return s_state.m_bAvailable && s_state.m_bEnhanced;
};


/*static*/ CEnhancedGraphics::PRESET CEnhancedGraphics::GetPreset(void)
{
    return s_state.m_preset;
};


/*static*/ const char* CEnhancedGraphics::GetPresetName(PRESET preset)
{
    switch (preset)
    {
    case PRESET_CLASSIC_PLUS: return "Classic+";
    case PRESET_ENHANCED:     return "Enhanced";
    case PRESET_HIGH:         return "High";
    case PRESET_CUSTOM:       return "Custom";
    default:                  return "?";
    };
};


/*static*/ const EGSETTINGS& CEnhancedGraphics::Get(void)
{
    return s_state.m_effective;
};


/*static*/ void CEnhancedGraphics::RegisterApplyCallback(APPLYCALLBACK pfnCallback)
{
    if (!pfnCallback)
        return;

    for (int32 i = 0; i < s_state.m_nApply; ++i)
    {
        if (s_state.m_apfnApply[i] == pfnCallback)
            return;
    };

    if (s_state.m_nApply < APPLY_MAX)
        s_state.m_apfnApply[s_state.m_nApply++] = pfnCallback;
};


/*static*/ void CEnhancedGraphics::Log(const char* pszFormat, ...)
{
    if (!s_state.m_pLog)
        return;

    char szBuffer[1024];
    va_list vl;
    va_start(vl, pszFormat);
    std::vsnprintf(szBuffer, sizeof(szBuffer), pszFormat, vl);
    va_end(vl);

    SYSTEMTIME st;
    GetLocalTime(&st);
    std::fprintf(s_state.m_pLog, "%02u:%02u:%02u.%03u %s\r\n",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, szBuffer);
    std::fflush(s_state.m_pLog);
};


/*static*/ bool CEnhancedGraphics::IsWaterTextureIncluded(const char* pszTextureName)
{
    return NameListContains(s_state.m_waterInclude, pszTextureName);
};


/*static*/ bool CEnhancedGraphics::IsWaterTextureExcluded(const char* pszTextureName)
{
    return NameListContains(s_state.m_waterExclude, pszTextureName);
};


/*static*/ bool CEnhancedGraphics::IsWaterCollisionFallbackEnabled(void)
{
    return s_state.m_bWaterCollisionFallback;
};


/*static*/ void CEnhancedGraphics::SetStageContext(const RwRGBA& rFogColor, bool bFog, const RwV3d& rvLightTravel)
{
    s_state.m_fogColor = rFogColor;
    s_state.m_bFog = bFog;

    RwV3d v = rvLightTravel;
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 0.0001f)
    {
        v.x /= len; v.y /= len; v.z /= len;
        s_state.m_vLightTravel = v;
    };

    ComputeShadowTint();

    if (s_state.m_bAvailable)
    {
        Log("STAGE fog=%d color=%02X%02X%02X light_travel=(%.2f %.2f %.2f) shadow_tint=(%.2f %.2f %.2f)",
            (bFog ? 1 : 0), rFogColor.red, rFogColor.green, rFogColor.blue,
            s_state.m_vLightTravel.x, s_state.m_vLightTravel.y, s_state.m_vLightTravel.z,
            s_state.m_afShadowTint[0], s_state.m_afShadowTint[1], s_state.m_afShadowTint[2]);
    };
};


/*static*/ void CEnhancedGraphics::GetToonShadowTint(float afRGB[3])
{
    afRGB[0] = s_state.m_afShadowTint[0];
    afRGB[1] = s_state.m_afShadowTint[1];
    afRGB[2] = s_state.m_afShadowTint[2];
};


/*static*/ void CEnhancedGraphics::GetLightTravel(RwV3d* pvTravel)
{
    *pvTravel = s_state.m_vLightTravel;
};


/*static*/ bool CEnhancedGraphics::WaterSetMesh(const WATERVERTEX* pVertex, int32 nVertex,
                                                const uint16* pIndex, int32 nIndex,
                                                const RwRGBA& rBaseColor)
{
#if defined(TMNT2_RWDRV_D3D9)
    WaterClear();

    if (!s_state.m_bAvailable || (nVertex <= 0) || (nIndex <= 0) || (nVertex > 65535))
        return false;

    CreateStaticResources();
    if (s_d3d.m_bWaterBroken)
        return false;

    IDirect3DDevice9* pDevice = Device();
    if (!pDevice)
        return false;

    UINT vbSize = static_cast<UINT>(nVertex) * sizeof(WATERVERTEX);
    UINT ibSize = static_cast<UINT>(nIndex) * sizeof(uint16);

    if (FAILED(pDevice->CreateVertexBuffer(vbSize, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &s_d3d.m_pWaterVB, nullptr)) ||
        FAILED(pDevice->CreateIndexBuffer(ibSize, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &s_d3d.m_pWaterIB, nullptr)))
    {
        Log("WATER buffer creation failed (%d vertices, %d indices)", nVertex, nIndex);
        WaterClear();
        return false;
    };

    void* pDst = nullptr;
    if (SUCCEEDED(s_d3d.m_pWaterVB->Lock(0, vbSize, &pDst, 0)))
    {
        std::memcpy(pDst, pVertex, vbSize);
        s_d3d.m_pWaterVB->Unlock();
    };

    if (SUCCEEDED(s_d3d.m_pWaterIB->Lock(0, ibSize, &pDst, 0)))
    {
        std::memcpy(pDst, pIndex, ibSize);
        s_d3d.m_pWaterIB->Unlock();
    };

    s_d3d.m_nWaterVertex = nVertex;
    s_d3d.m_nWaterIndex = nIndex;
    s_d3d.m_afWaterBase[0] = static_cast<float>(rBaseColor.red) / 255.0f;
    s_d3d.m_afWaterBase[1] = static_cast<float>(rBaseColor.green) / 255.0f;
    s_d3d.m_afWaterBase[2] = static_cast<float>(rBaseColor.blue) / 255.0f;
    s_d3d.m_fWaterTime = 0.0f;

    Log("WATER mesh ready: %d vertices, %d triangles, base colour %02X%02X%02X",
        nVertex, nIndex / 3, rBaseColor.red, rBaseColor.green, rBaseColor.blue);
    return true;
#else /* defined(TMNT2_RWDRV_D3D9) */
    return false;
#endif /* defined(TMNT2_RWDRV_D3D9) */
};


/*static*/ void CEnhancedGraphics::WaterClear(void)
{
#if defined(TMNT2_RWDRV_D3D9)
    if (s_d3d.m_pWaterVB || s_d3d.m_pWaterIB)
    {
        /* RenderWare caches the bound buffers by pointer: unbind before releasing. */
        RwD3D9SetStreamSource(0, nullptr, 0, 0);
        RwD3D9SetIndices(nullptr);
    };

    SafeRelease(s_d3d.m_pWaterVB);
    SafeRelease(s_d3d.m_pWaterIB);
    s_d3d.m_nWaterVertex = 0;
    s_d3d.m_nWaterIndex = 0;
#endif /* defined(TMNT2_RWDRV_D3D9) */
};


/*static*/ void CEnhancedGraphics::WaterAdvanceTime(float dt)
{
#if defined(TMNT2_RWDRV_D3D9)
    if ((dt > 0.0f) && (dt < 1.0f))
    {
        s_d3d.m_fWaterTime += dt;
        /* Every scroll speed and band rate is a multiple of 1/1000 per second, so wrapping at
         * 1000 s is seamless and keeps float precision for long sessions. */
        if (s_d3d.m_fWaterTime >= 1000.0f)
            s_d3d.m_fWaterTime -= 1000.0f;
    };
#endif /* defined(TMNT2_RWDRV_D3D9) */
};


/*static*/ void CEnhancedGraphics::WaterDraw(void)
{
#if defined(TMNT2_RWDRV_D3D9)
    const EGSETTINGS& e = s_state.m_effective;
    if (!e.m_bWater || s_d3d.m_bWaterBroken || !s_d3d.m_pWaterVB || !s_d3d.m_pWaterIB || (e.m_fWaterStrength <= 0.0f))
        return;

    IDirect3DDevice9* pDevice = Device();
    RwCamera* pCamera = RwCameraGetCurrentCamera();
    if (!pDevice || !pCamera)
        return;

    LONGLONG t0 = Qpc();
    QueryStamp(0);

    /* fog as RenderWare set it for the world */
    RwUInt32 fogEnable = 0, fogColor = 0, fogStartBits = 0, fogEndBits = 0;
    RwD3D9GetRenderState(D3DRS_FOGENABLE, &fogEnable);
    RwD3D9GetRenderState(D3DRS_FOGCOLOR, &fogColor);
    RwD3D9GetRenderState(D3DRS_FOGSTART, &fogStartBits);
    RwD3D9GetRenderState(D3DRS_FOGEND, &fogEndBits);
    if (fogEnable == RW_STATE_UNKNOWN)
        fogEnable = FALSE;
    float fFogStart, fFogEnd;
    std::memcpy(&fFogStart, &fogStartBits, sizeof(float));
    std::memcpy(&fFogEnd, &fogEndBits, sizeof(float));
    if (!(fFogEnd > fFogStart))
    {
        fFogStart = RwCameraGetFogDistance(pCamera);
        fFogEnd = RwCameraGetFarClipPlane(pCamera);
    };

    /* v1.1.1: RenderWare's D3D9 driver keeps the camera matrices in these globals for its
     * vertex shaders and never sets D3DTS_VIEW / D3DTS_PROJECTION, so RwD3D9GetTransform
     * returned identity and the water overlay was drawn off screen on every stage. */
    D3DMATRIX view = _RwD3D9D3D9ViewTransform;
    D3DMATRIX proj = _RwD3D9D3D9ProjTransform;

    float vp[4][4];
    for (int32 r = 0; r < 4; ++r)
    {
        for (int32 c = 0; c < 4; ++c)
        {
            vp[r][c] = (view.m[r][0] * proj.m[0][c]) + (view.m[r][1] * proj.m[1][c]) +
                       (view.m[r][2] * proj.m[2][c]) + (view.m[r][3] * proj.m[3][c]);
        };
    };

    float vsConst[5][4];
    for (int32 c = 0; c < 4; ++c)
    {
        vsConst[c][0] = vp[0][c];
        vsConst[c][1] = vp[1][c];
        vsConst[c][2] = vp[2][c];
        vsConst[c][3] = vp[3][c];
    };
    vsConst[4][0] = 0.02f;  /* lift above the original surface: avoids z-fighting */
    vsConst[4][1] = vsConst[4][2] = vsConst[4][3] = 0.0f;

    const RwV3d* pCamPos = &RwFrameGetLTM(RwCameraGetFrame(pCamera))->pos;
    const float* pBase = s_d3d.m_afWaterBase;
    const float fogR = static_cast<float>((fogColor >> 16) & 0xFF) / 255.0f;
    const float fogG = static_cast<float>((fogColor >> 8) & 0xFF) / 255.0f;
    const float fogB = static_cast<float>(fogColor & 0xFF) / 255.0f;

    float skyLo[3], skyHi[3];
    if (s_state.m_bFog)
    {
        skyLo[0] = static_cast<float>(s_state.m_fogColor.red) / 255.0f;
        skyLo[1] = static_cast<float>(s_state.m_fogColor.green) / 255.0f;
        skyLo[2] = static_cast<float>(s_state.m_fogColor.blue) / 255.0f;
    }
    else
    {
        for (int32 i = 0; i < 3; ++i)
            skyLo[i] = Clampf(pBase[i] * 1.5f + 0.15f, 0.0f, 1.0f);
    };

    static const float s_afSkyTop[3] = { 0.85f, 0.92f, 1.0f };
    for (int32 i = 0; i < 3; ++i)
        skyHi[i] = Lerpf(skyLo[i], s_afSkyTop[i], 0.5f);

    const float warm = 0.06f;
    float psConst[11][4] =
    {
        { pCamPos->x, pCamPos->y, pCamPos->z, s_d3d.m_fWaterTime },
        { -s_state.m_vLightTravel.x, -s_state.m_vLightTravel.y, -s_state.m_vLightTravel.z, (e.m_bWaterDetail ? 96.0f : 64.0f) },
        { Clampf(pBase[0] * 1.20f + 0.03f, 0.0f, 1.0f), Clampf(pBase[1] * 1.20f + 0.05f, 0.0f, 1.0f), Clampf(pBase[2] * 1.20f + 0.06f, 0.0f, 1.0f), 0.16f },
        { pBase[0] * 0.55f, pBase[1] * 0.60f, pBase[2] * 0.68f, 0.42f },
        { skyLo[0], skyLo[1], skyLo[2], e.m_fWaterReflection },
        { skyHi[0], skyHi[1], skyHi[2], e.m_fWaterSpecular },
        { 1.0f, 1.0f - warm * 0.5f, 1.0f - warm, e.m_fWaterFoam },
        { fogR, fogG, fogB, (fogEnable ? 1.0f : 0.0f) },
        { fFogStart, fFogEnd, e.m_fWaterWaveScale, e.m_fWaterWaveStrength },
        { 0.010f, 0.0065f, -0.008f, 0.0105f },     /* v1.1.2: slow drift, stage water is near still */
        { e.m_fWaterStrength, (e.m_bWaterDetail ? 1.0f : 0.0f), 0.0f, 0.0f },
    };

    STATEGUARD guard;
    guard.SaveSamplers();
    guard.Set(D3DRS_ZENABLE, D3DZB_TRUE);
    guard.Set(D3DRS_ZWRITEENABLE, FALSE);
    guard.Set(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    guard.Set(D3DRS_ALPHABLENDENABLE, TRUE);
    /* v1.1.2: modulate 2x (dest * 2 * src). The shader's 0.5 leaves the game's own water as it
     * is; the overlay brightens or darkens it relative to the stage's lighting. */
    guard.Set(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
    guard.Set(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
    guard.Set(D3DRS_CULLMODE, D3DCULL_NONE);
    guard.Set(D3DRS_FOGENABLE, FALSE);
    guard.Set(D3DRS_ALPHATESTENABLE, FALSE);
    guard.Set(D3DRS_STENCILENABLE, FALSE);
    guard.Set(D3DRS_COLORWRITEENABLE, 0x0000000F);
    guard.Set(D3DRS_SRGBWRITEENABLE, FALSE);

    _rwD3D9SetVertexDeclaration(s_d3d.m_pWaterDecl);
    _rwD3D9SetVertexShader(s_d3d.m_pWaterVS);
    _rwD3D9SetPixelShader(s_d3d.m_pWaterPS);
    _rwD3D9SetVertexShaderConstant(0, vsConst, 5);
    _rwD3D9SetPixelShaderConstant(0, psConst, 11);

    RwD3D9SetTexture(nullptr, 0);   /* sync RenderWare's texture cache before binding a raw texture */
    pDevice->SetTexture(0, s_d3d.m_pWaveTex);
    SetSampler(0, D3DTADDRESS_WRAP, D3DTEXF_LINEAR, D3DTEXF_LINEAR);

    RwD3D9SetStreamSource(0, s_d3d.m_pWaterVB, 0, sizeof(WATERVERTEX));
    RwD3D9SetIndices(s_d3d.m_pWaterIB);
    _rwD3D9DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, s_d3d.m_nWaterVertex, 0, s_d3d.m_nWaterIndex / 3);

    RwD3D9SetStreamSource(0, nullptr, 0, 0);
    RwD3D9SetIndices(nullptr);
    pDevice->SetTexture(0, nullptr);
    _rwD3D9SetVertexShader(nullptr);
    _rwD3D9SetPixelShader(nullptr);
    guard.RestoreSamplers();
    guard.Restore();

    QueryStamp(1);
    s_state.m_dCpuMsSum += QpcToMs(Qpc() - t0);
#endif /* defined(TMNT2_RWDRV_D3D9) */
};


/*static*/ void CEnhancedGraphics::PostProcess(void)
{
#if defined(TMNT2_RWDRV_D3D9)
    const EGSETTINGS& e = s_state.m_effective;
    if ((!e.m_bBloom && !e.m_bGrade && !e.m_bFXAA) || s_d3d.m_bPostBroken)
        return;

    CreateStaticResources();
    if (s_d3d.m_bPostBroken)
        return;

    IDirect3DDevice9* pDevice = Device();
    if (!pDevice)
        return;

    LONGLONG t0 = Qpc();

    IDirect3DSurface9* pBackRT = nullptr;
    if (FAILED(pDevice->GetRenderTarget(0, &pBackRT)) || !pBackRT)
        return;

    D3DSURFACE_DESC desc;
    if (FAILED(pBackRT->GetDesc(&desc)) || !EnsureRenderTargets(pDevice, desc.Width, desc.Height, desc.Format))
    {
        pBackRT->Release();
        return;
    };

    QueryStamp(2);

    IDirect3DSurface9* pSceneSurf = nullptr;
    IDirect3DSurface9* pBloomSurfA = nullptr;
    IDirect3DSurface9* pBloomSurfB = nullptr;
    s_d3d.m_pSceneTex->GetSurfaceLevel(0, &pSceneSurf);
    s_d3d.m_pBloomTexA->GetSurfaceLevel(0, &pBloomSurfA);
    s_d3d.m_pBloomTexB->GetSurfaceLevel(0, &pBloomSurfB);

    /* Resolve the frame so far (3D world, characters, effects; no HUD yet). Works for MSAA too. */
    HRESULT hrCopy = pDevice->StretchRect(pBackRT, nullptr, pSceneSurf, nullptr, D3DTEXF_NONE);
    if (FAILED(hrCopy))
    {
        Log("POST StretchRect failed hr=0x%08lX: post disabled", static_cast<unsigned long>(hrCopy));
        s_d3d.m_bPostBroken = true;
    }
    else
    {
        D3DVIEWPORT9 vpSaved;
        pDevice->GetViewport(&vpSaved);

        IDirect3DSurface9* pDepth = nullptr;
        pDevice->GetDepthStencilSurface(&pDepth);
        pDevice->SetDepthStencilSurface(nullptr);

        STATEGUARD guard;
        guard.SaveSamplers();
        guard.Set(D3DRS_ZENABLE, D3DZB_FALSE);
        guard.Set(D3DRS_ZWRITEENABLE, FALSE);
        guard.Set(D3DRS_ALPHABLENDENABLE, FALSE);
        guard.Set(D3DRS_ALPHATESTENABLE, FALSE);
        guard.Set(D3DRS_CULLMODE, D3DCULL_NONE);
        guard.Set(D3DRS_FOGENABLE, FALSE);
        guard.Set(D3DRS_STENCILENABLE, FALSE);
        guard.Set(D3DRS_SCISSORTESTENABLE, FALSE);
        guard.Set(D3DRS_COLORWRITEENABLE, 0x0000000F);
        guard.Set(D3DRS_SRGBWRITEENABLE, FALSE);

        _rwD3D9SetVertexDeclaration(s_d3d.m_pFullscreenDecl);
        _rwD3D9SetVertexShader(s_d3d.m_pFullscreenVS);
        RwD3D9SetTexture(nullptr, 0);
        RwD3D9SetTexture(nullptr, 1);

        const UINT w = desc.Width, h = desc.Height;
        const UINT qw = (w / 4) ? (w / 4) : 1;
        const UINT qh = (h / 4) ? (h / 4) : 1;
        const bool bBloom = e.m_bBloom && (e.m_fBloomIntensity > 0.0f);

        if (bBloom)
        {
            /* bright pass + downsample to quarter resolution */
            float c0[4] = { 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h), e.m_fBloomThreshold, 0.25f };
            _rwD3D9SetPixelShader(s_d3d.m_pBrightPS);
            _rwD3D9SetPixelShaderConstant(0, c0, 1);
            pDevice->SetTexture(0, s_d3d.m_pSceneTex);
            SetSampler(0, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR, D3DTEXF_NONE);
            DrawFullscreen(pDevice, pBloomSurfA, qw, qh);

            /* blur horizontally into B, vertically back into A */
            _rwD3D9SetPixelShader(s_d3d.m_pBlurPS);
            float ch[4] = { 1.0f / static_cast<float>(qw), 0.0f, 0.0f, 0.0f };
            _rwD3D9SetPixelShaderConstant(0, ch, 1);
            pDevice->SetTexture(0, s_d3d.m_pBloomTexA);
            DrawFullscreen(pDevice, pBloomSurfB, qw, qh);

            float cv[4] = { 0.0f, 1.0f / static_cast<float>(qh), 0.0f, 0.0f };
            _rwD3D9SetPixelShaderConstant(0, cv, 1);
            pDevice->SetTexture(0, s_d3d.m_pBloomTexB);
            DrawFullscreen(pDevice, pBloomSurfA, qw, qh);
        };

        /* composite back onto the frame */
        const float fGrade = (e.m_bGrade ? 1.0f : 0.0f);
        const float fSplit = e.m_fSplitTone * fGrade;
        float cc[4][4] =
        {
            { 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h), (bBloom ? e.m_fBloomIntensity : 0.0f), (e.m_bFXAA ? 1.0f : 0.0f) },
            { Lerpf(1.0f, e.m_fContrast, fGrade), Lerpf(1.0f, e.m_fSaturation, fGrade), e.m_fVignette * fGrade, 0.0f },
            { -0.020f, -0.004f, 0.030f, 0.60f * fSplit },   /* cool shadows   */
            {  0.020f,  0.008f, -0.016f, 0.45f * fSplit },  /* warm highlights */
        };
        _rwD3D9SetPixelShader(s_d3d.m_pCompositePS);
        _rwD3D9SetPixelShaderConstant(0, cc, 4);
        pDevice->SetTexture(0, s_d3d.m_pSceneTex);
        pDevice->SetTexture(1, (bBloom ? s_d3d.m_pBloomTexA : s_d3d.m_pSceneTex));
        SetSampler(0, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR, D3DTEXF_NONE);
        SetSampler(1, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR, D3DTEXF_NONE);
        DrawFullscreen(pDevice, pBackRT, w, h);

        /* restore */
        pDevice->SetTexture(0, nullptr);
        pDevice->SetTexture(1, nullptr);
        _rwD3D9SetVertexShader(nullptr);
        _rwD3D9SetPixelShader(nullptr);
        pDevice->SetRenderTarget(0, pBackRT);
        pDevice->SetDepthStencilSurface(pDepth);
        pDevice->SetViewport(&vpSaved);
        guard.RestoreSamplers();
        guard.Restore();

        SafeRelease(pDepth);
    };

    SafeRelease(pSceneSurf);
    SafeRelease(pBloomSurfA);
    SafeRelease(pBloomSurfB);
    pBackRT->Release();

    QueryStamp(3);
    s_state.m_dCpuMsSum += QpcToMs(Qpc() - t0);
#endif /* defined(TMNT2_RWDRV_D3D9) */
};


/*static*/ bool CEnhancedGraphics::GetToast(char* pszBuffer, int32 nBufferSize, float* pfAlpha)
{
    if (!s_state.m_bAvailable || !s_state.m_toastStart || !s_state.m_szToast[0])
        return false;

    double dMs = QpcToMs(Qpc() - s_state.m_toastStart);
    if (dMs >= 2500.0)
    {
        s_state.m_toastStart = 0;
        return false;
    };

    std::snprintf(pszBuffer, static_cast<size_t>(nBufferSize), "%s", s_state.m_szToast);
    *pfAlpha = (dMs < 2000.0) ? 1.0f : static_cast<float>((2500.0 - dMs) / 500.0);
    return true;
};


/*=========================================================================
 *  Live editing (Debug Tools "Remaster" tab)
 *=======================================================================*/

/*static*/ void CEnhancedGraphics::SetEnhanced(bool bEnhanced)
{
    if (!s_state.m_bAvailable || (s_state.m_bEnhanced == bEnhanced))
        return;

    s_state.m_bEnhanced = bEnhanced;
    UpdateEffective("menu-toggle");
    ShowToast();
};


/*static*/ void CEnhancedGraphics::SetPreset(PRESET preset)
{
    if (!s_state.m_bAvailable || (preset < 0) || (preset >= PRESET_NUM))
        return;

    s_state.m_preset = preset;
    s_state.m_bEnhanced = true;
    std::memset(s_state.m_abOverride, 0, sizeof(s_state.m_abOverride));
    UpdateEffective("menu-preset");
    ShowToast();
};


/*static*/ int32 CEnhancedGraphics::FindKey(const char* pszName)
{
    for (int32 i = 0; i < COUNT_OF(s_aKeyInfo); ++i)
    {
        if (_stricmp(s_aKeyInfo[i].m_pszName, pszName) == 0)
            return i;
    };

    return -1;
};


/*static*/ bool CEnhancedGraphics::IsKeyBool(int32 key)
{
    return (key >= 0) && (key < COUNT_OF(s_aKeyInfo)) && (s_aKeyInfo[key].m_type == KEY_BOOL);
};


/*static*/ float CEnhancedGraphics::GetKeyMin(int32 key)
{
    return ((key >= 0) && (key < COUNT_OF(s_aKeyInfo))) ? s_aKeyInfo[key].m_fMin : 0.0f;
};


/*static*/ float CEnhancedGraphics::GetKeyMax(int32 key)
{
    return ((key >= 0) && (key < COUNT_OF(s_aKeyInfo))) ? s_aKeyInfo[key].m_fMax : 1.0f;
};


/*static*/ float CEnhancedGraphics::GetKeyValue(int32 key)
{
    if ((key < 0) || (key >= COUNT_OF(s_aKeyInfo)))
        return 0.0f;

    const KEYINFO& rKey = s_aKeyInfo[key];
    const uint8* pBase = reinterpret_cast<const uint8*>(&s_state.m_configured);
    if (rKey.m_type == KEY_BOOL)
        return (*reinterpret_cast<const bool*>(pBase + rKey.m_offset) ? 1.0f : 0.0f);

    return *reinterpret_cast<const float*>(pBase + rKey.m_offset);
};


/*static*/ void CEnhancedGraphics::SetKeyValue(int32 key, float fValue)
{
    if (!s_state.m_bAvailable || (key < 0) || (key >= COUNT_OF(s_aKeyInfo)))
        return;

    const KEYINFO& rKey = s_aKeyInfo[key];
    uint8* pBase = reinterpret_cast<uint8*>(&s_state.m_custom);
    if (rKey.m_type == KEY_BOOL)
        *reinterpret_cast<bool*>(pBase + rKey.m_offset) = (fValue >= 0.5f);
    else
        *reinterpret_cast<float*>(pBase + rKey.m_offset) = Clampf(fValue, rKey.m_fMin, rKey.m_fMax);

    s_state.m_abOverride[key] = true;

    /* an edit is meant to be seen: switch to ENHANCED if ORIGINAL was showing */
    bool bWasOriginal = !s_state.m_bEnhanced;
    s_state.m_bEnhanced = true;
    UpdateEffective("menu-edit");
    if (bWasOriginal)
        ShowToast();
};


/*static*/ bool CEnhancedGraphics::IsKeyOverridden(int32 key)
{
    return (key >= 0) && (key < COUNT_OF(s_aKeyInfo)) && s_state.m_abOverride[key];
};


/*static*/ void CEnhancedGraphics::ClearKeyOverride(int32 key)
{
    if ((key < 0) || (key >= COUNT_OF(s_aKeyInfo)) || !s_state.m_abOverride[key])
        return;

    s_state.m_abOverride[key] = false;
    UpdateEffective("menu-reset-item");
};


/*static*/ void CEnhancedGraphics::ClearAllOverrides(void)
{
    std::memset(s_state.m_abOverride, 0, sizeof(s_state.m_abOverride));
    UpdateEffective("menu-reset-tab");
};


/*static*/ int32 CEnhancedGraphics::GetOverrideCount(void)
{
    int32 n = 0;
    for (int32 i = 0; i < COUNT_OF(s_aKeyInfo); ++i)
        n += (s_state.m_abOverride[i] ? 1 : 0);
    return n;
};


/*static*/ bool CEnhancedGraphics::SaveIni(void)
{
    wchar_t wszPath[MAX_PATH] = {};
    if (!s_state.m_bAvailable || !BuildPath(wszPath, MAX_PATH, L"TMNT2_graphics.ini"))
        return false;

    FILE* pFile = nullptr;
    if ((_wfopen_s(&pFile, wszPath, L"wb") != 0) || !pFile)
    {
        Log("INI save failed (file not writable)");
        return false;
    };

    std::fprintf(pFile, "; TMNT2 Enhanced Graphics v1.1 settings (saved from Debug Tools > REMASTER)\r\n");
    std::fprintf(pFile, "; F8 = original / enhanced   Shift+F8 = next preset   Ctrl+F8 = reload this file\r\n\r\n");
    std::fprintf(pFile, "[Graphics]\r\n; ClassicPlus | Enhanced | High | Custom\r\nPreset=%s\r\n", PresetIniName(s_state.m_preset));
    std::fprintf(pFile, "StartEnhanced=%d\r\nToast=%d\r\nPerfLog=%d\r\n\r\n",
                 (s_state.m_bEnhanced ? 1 : 0), (s_state.m_bToastEnabled ? 1 : 0), (s_state.m_bPerfLog ? 1 : 0));

    std::fprintf(pFile, "[Overrides]\r\n; Active keys override the preset; ';' lines show the preset value for reference.\r\n");
    for (int32 i = 0; i < COUNT_OF(s_aKeyInfo); ++i)
    {
        const KEYINFO& rKey = s_aKeyInfo[i];
        const char* pszPrefix = (s_state.m_abOverride[i] ? "" : ";");
        if (rKey.m_type == KEY_BOOL)
            std::fprintf(pFile, "%s%s=%d\r\n", pszPrefix, rKey.m_pszName, (GetKeyValue(i) >= 0.5f) ? 1 : 0);
        else
            std::fprintf(pFile, "%s%s=%.2f\r\n", pszPrefix, rKey.m_pszName, GetKeyValue(i));
    };

    std::fprintf(pFile, "\r\n[Water]\r\nInclude=");
    for (int32 i = 0; i < s_state.m_waterInclude.m_nCount; ++i)
        std::fprintf(pFile, "%s%s", (i ? "," : ""), s_state.m_waterInclude.m_aszName[i]);
    std::fprintf(pFile, "\r\nExclude=");
    for (int32 i = 0; i < s_state.m_waterExclude.m_nCount; ++i)
        std::fprintf(pFile, "%s%s", (i ? "," : ""), s_state.m_waterExclude.m_aszName[i]);
    std::fprintf(pFile, "\r\nCollisionFallback=%d\r\n", (s_state.m_bWaterCollisionFallback ? 1 : 0));

    std::fclose(pFile);
    Log("INI saved: preset=%s overrides=%d", PresetIniName(s_state.m_preset), GetOverrideCount());
    std::snprintf(s_state.m_szToast, sizeof(s_state.m_szToast), "GRAPHICS: SETTINGS SAVED");
    s_state.m_toastStart = Qpc();
    return true;
};


/*static*/ void CEnhancedGraphics::ReloadIni(void)
{
    if (!s_state.m_bAvailable)
        return;

    bool bEnhanced = s_state.m_bEnhanced;
    LoadIni();
    s_state.m_bEnhanced = bEnhanced;
    UpdateEffective("menu-reload");
    ShowToast();
};


/*static*/ void CEnhancedGraphics::SetWaterMaterialCount(int32 nMaterial)
{
    s_state.m_nWaterMaterial = nMaterial;
};


/*static*/ void CEnhancedGraphics::GetWaterStats(int32* pnMaterial, int32* pnTriangle, bool* pbBroken)
{
    *pnMaterial = s_state.m_nWaterMaterial;
#if defined(TMNT2_RWDRV_D3D9)
    *pnTriangle = s_d3d.m_nWaterIndex / 3;
    *pbBroken = s_d3d.m_bWaterBroken;
#else /* defined(TMNT2_RWDRV_D3D9) */
    *pnTriangle = 0;
    *pbBroken = true;
#endif /* defined(TMNT2_RWDRV_D3D9) */
};


/*static*/ bool CEnhancedGraphics::GetLastPerf(float* pfFps, float* pfPostMs, float* pfWaterMs)
{
    *pfFps = s_state.m_fLastFps;
    *pfPostMs = s_state.m_fLastPostMs;
    *pfWaterMs = s_state.m_fLastWaterMs;
    return s_state.m_bHaveLastPerf;
};


/*static*/ bool CEnhancedGraphics::IsPostAvailable(void)
{
#if defined(TMNT2_RWDRV_D3D9)
    return !s_d3d.m_bPostBroken;
#else /* defined(TMNT2_RWDRV_D3D9) */
    return false;
#endif /* defined(TMNT2_RWDRV_D3D9) */
};
