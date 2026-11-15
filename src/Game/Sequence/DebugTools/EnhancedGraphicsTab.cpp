#include "EnhancedGraphicsTab.hpp"

#if defined(TARGET_PC)
#include "System/PC/PCEnhancedGraphics.hpp"
#endif /* defined(TARGET_PC) */

#include <cstdio>
#include <cstring>
#include <cmath>


#if defined(TARGET_PC)

namespace
{
    enum ROWTYPE
    {
        ROW_MASTER = 0,
        ROW_PRESET,
        ROW_KEY,
        ROW_READ_WATER,
        ROW_READ_PERF,
        ROW_SAVE,
        ROW_RELOAD,
        ROW_RESET,
    };

    struct ROWINFO
    {
        ROWTYPE     m_type;
        const char* m_pszHeader;    /* section title shown above this row, or nullptr */
        const char* m_pszLabel;
        const char* m_pszKey;       /* TMNT2_graphics.ini key for ROW_KEY */
        float       m_fStep;
        const char* m_pszScope;
        const char* m_pszHelp;
    };

    static const ROWINFO s_aRow[] =
    {
        { ROW_MASTER, "BEFORE / AFTER", "Enhanced graphics (F8)", nullptr, 0.0f, "LIVE - WHOLE GAME",
          "OFF runs every original graphics path exactly as shipped. ON applies the settings below. F8 switches at any time, also with this menu closed." },
        { ROW_PRESET, nullptr, "Preset (Shift+F8)", nullptr, 0.0f, "LIVE - WHOLE GAME",
          "Classic+ = original look (no remaster features). Enhanced = default remaster. High = stronger water, highlights and FXAA. Custom = Enhanced plus your edits. Changing the preset clears the edits made below (they are marked * and counted here)." },
        { ROW_READ_PERF, nullptr, "Performance", nullptr, 0.0f, "LIVE READOUT",
          "Smoothed frame rate, and the GPU time of the post pass and the water pass measured with Direct3D timestamp queries. Compare by pressing F8 and waiting a few seconds. 10-second summaries also go to TMNT2_graphics.log." },

        { ROW_KEY, "TOON LIGHTING", "Toon lighting",           "ToonLighting",   0.0f,  "LIVE - CHARACTERS", "Remapped toon shading ramps: crisper, deeper light/shadow split, stage-tinted shadows and a wider lit side. Ink outlines are untouched; OFF puts the original ramps back. The light rig below also works through these ramps." },
        { ROW_KEY, nullptr,         "Light/shadow contrast",   "ToonContrast",   0.05f, "LIVE - CHARACTERS", "Moves the light/shadow split and deepens the shadow side (up to about 45% darker). 0 = original ramp." },
        { ROW_KEY, nullptr,         "Shadow tint",             "ToonShadowTint", 0.05f, "LIVE - CHARACTERS", "Strength of the stage-coloured hue in shaded areas (cool on most stages). 0 = neutral shadows." },
        { ROW_KEY, nullptr,         "Lit-side width",          "ToonHighlight",  0.05f, "LIVE - CHARACTERS", "Widens the lit side by lifting the mid tones toward the lit colour. 0 = original." },
        { ROW_KEY, nullptr,         "Light rig",               "LightRig",       0.0f,  "LIVE - CHARACTERS", "TMNT2's cel shader ignores light colour and the world is prelit, so the rig acts on the character toon ramps (needs Toon lighting ON): shadow level, lit level and warm/cool split. It also scales the stage lights for the few non-toon lit models." },
        { ROW_KEY, nullptr,         "Ambient light",           "AmbientScale",   0.02f, "LIVE - CHARACTERS", "Brightness of the shadow side. Below 1 = darker shadows, above 1 = softer. 1 = original." },
        { ROW_KEY, nullptr,         "Key light",               "KeyLight",       0.02f, "LIVE - CHARACTERS", "Below 1 dims the lit side; above 1 brightens the area around the light/shadow edge. 1 = original." },
        { ROW_KEY, nullptr,         "Warm key / cool fill",    "LightWarmth",    0.05f, "LIVE - CHARACTERS", "Warm lit side and cool shadow side. 0 = neutral." },

        { ROW_KEY, "SHADOWS",       "Soft shadows",            "Shadows",        0.0f,  "LIVE - CHARACTERS AND GIMMICKS", "Soft-edged blob shadows that widen and fade with jump height but never vanish, so the landing spot stays readable. OFF = original shadow." },
        { ROW_KEY, nullptr,         "Shadow opacity",          "ShadowOpacity",  0.02f, "LIVE", "Darkness of the soft shadow on the ground." },
        { ROW_KEY, nullptr,         "Shadow size",             "ShadowSize",     0.02f, "LIVE", "Size relative to the original shadow." },
        { ROW_KEY, nullptr,         "Contact shadow",          "ContactShadow",  0.0f,  "LIVE", "Small dark core right under the feet that disappears within about one unit of jump height." },
        { ROW_KEY, nullptr,         "Contact darkness",        "ContactOpacity", 0.05f, "LIVE", "Darkness of the contact core." },
        { ROW_KEY, nullptr,         "Offset from light",       "ShadowSunOffset",0.01f, "LIVE", "Pushes the soft shadow slightly away from the stage light, along the ground. 0 = centred like the original." },

        { ROW_KEY, "WATER",         "Stylized water",          "Water",          0.0f,  "LIVE - WATER STAGES", "Animated overlay with waves, sky reflection, toon highlights and shoreline foam on the surfaces detected as water. Collision, splashes and ripples are unchanged." },
        { ROW_READ_WATER, nullptr,  "Detected on this stage",  nullptr,          0.0f,  "READOUT", "Water materials and triangles found when the stage loaded. Every candidate and the reason for the decision are in TMNT2_graphics.log; [Water] Include / Exclude in TMNT2_graphics.ini override it by texture name." },
        { ROW_KEY, nullptr,         "Water strength",          "WaterStrength",  0.05f, "LIVE", "Overall opacity of the overlay. 0 hides it." },
        { ROW_KEY, nullptr,         "Reflection",              "WaterReflection",0.05f, "LIVE", "Sky/fog reflection at glancing angles." },
        { ROW_KEY, nullptr,         "Highlights",              "WaterSpecular",  0.05f, "LIVE", "Crisp cartoon light glints on the waves." },
        { ROW_KEY, nullptr,         "Shore foam",              "WaterFoam",      0.05f, "LIVE", "Animated foam bands along the edges of the water." },
        { ROW_KEY, nullptr,         "Wave size",               "WaterWaveScale", 0.01f, "LIVE", "Texture repeats per world unit: lower = bigger waves." },
        { ROW_KEY, nullptr,         "Wave strength",           "WaterWaveStrength",0.05f,"LIVE", "How strongly the waves bend reflections and highlights." },
        { ROW_KEY, nullptr,         "Fine ripple layer",       "WaterDetail",    0.0f,  "LIVE", "Adds a third, smaller wave layer (High preset)." },

        { ROW_KEY, "SCREEN AND TEXTURES", "Bloom",             "Bloom",          0.0f,  "LIVE - 3D SCENE (HUD EXCLUDED)", "Soft glow around bright areas, computed at quarter resolution." },
        { ROW_KEY, nullptr,         "Bloom amount",            "BloomIntensity", 0.01f, "LIVE", "Glow strength." },
        { ROW_KEY, nullptr,         "Bloom threshold",         "BloomThreshold", 0.02f, "LIVE", "Brightness needed to glow: higher = only highlights." },
        { ROW_KEY, nullptr,         "Colour grade",            "ColorGrade",     0.0f,  "LIVE - 3D SCENE (HUD EXCLUDED)", "Slight contrast and saturation, cool shadows / warm highlights, light vignette." },
        { ROW_KEY, nullptr,         "Contrast",                "Contrast",       0.01f, "LIVE", "1 = unchanged." },
        { ROW_KEY, nullptr,         "Saturation",              "Saturation",     0.01f, "LIVE", "1 = unchanged." },
        { ROW_KEY, nullptr,         "Cool/warm split tone",    "SplitTone",      0.05f, "LIVE", "0 = neutral." },
        { ROW_KEY, nullptr,         "Vignette",                "Vignette",       0.01f, "LIVE", "Darkening toward the corners. 0 = none." },
        { ROW_KEY, nullptr,         "FXAA edge smoothing",     "FXAA",           0.0f,  "LIVE", "Light anti-aliasing on the final image. With MSAA on it is usually not needed." },
        { ROW_KEY, nullptr,         "Trilinear filtering",     "Trilinear",      0.0f,  "LIVE - TEXTURES", "Trilinear filtering for textures that have mip levels. TMNT2's own textures ship without mip levels (the log counts them), so this only affects mipmapped textures added by other mods. OFF = the game's original filtering." },

        { ROW_SAVE,   "SETTINGS FILE", "Save to TMNT2_graphics.ini", nullptr,   0.0f,  "FILE", "Writes the preset, ON/OFF side and every edit (*) so they load at the next start." },
        { ROW_RELOAD, nullptr,      "Reload TMNT2_graphics.ini", nullptr,        0.0f,  "FILE", "Discards unsaved edits and reads the file again (same as Ctrl+F8)." },
        { ROW_RESET,  nullptr,      "Clear edits (back to preset)", nullptr,     0.0f,  "LIVE", "Removes every edit marked *; same as Y (reset tab)." },
    };

    static const int32 ROW_NUM = static_cast<int32>(sizeof(s_aRow) / sizeof(s_aRow[0]));

    static int32 s_aiKey[ROW_NUM];
    static bool s_bKeysResolved = false;


    int32 KeyOf(int32 index)
    {
        if (!s_bKeysResolved)
        {
            for (int32 i = 0; i < ROW_NUM; ++i)
                s_aiKey[i] = (s_aRow[i].m_pszKey ? CEnhancedGraphics::FindKey(s_aRow[i].m_pszKey) : -1);
            s_bKeysResolved = true;
        };

        return ((index >= 0) && (index < ROW_NUM)) ? s_aiKey[index] : -1;
    };


    bool IsFloatRow(int32 index)
    {
        int32 key = KeyOf(index);
        return (key >= 0) && !CEnhancedGraphics::IsKeyBool(key);
    };
};


/*static*/ bool CEnhancedGraphicsTab::IsVisible(void)
{
    return CEnhancedGraphics::IsAvailable();
};


/*static*/ int32 CEnhancedGraphicsTab::GetItemNum(void)
{
    return ROW_NUM;
};


/*static*/ bool CEnhancedGraphicsTab::IsNumericItem(int32 index)
{
    return IsFloatRow(index);
};


/*static*/ bool CEnhancedGraphicsTab::GetItemFraction(int32 index, float* pfFraction)
{
    if (!IsFloatRow(index))
        return false;

    int32 key = KeyOf(index);
    float lo = CEnhancedGraphics::GetKeyMin(key), hi = CEnhancedGraphics::GetKeyMax(key);
    float v = CEnhancedGraphics::GetKeyValue(key);
    *pfFraction = (hi > lo) ? Clamp((v - lo) / (hi - lo), 0.0f, 1.0f) : 1.0f;
    return true;
};


/*static*/ bool CEnhancedGraphicsTab::GetItemRange(int32 index, float* pfMin, float* pfMax, float* pfValue)
{
    if (!IsFloatRow(index))
        return false;

    int32 key = KeyOf(index);
    *pfMin = CEnhancedGraphics::GetKeyMin(key);
    *pfMax = CEnhancedGraphics::GetKeyMax(key);
    *pfValue = CEnhancedGraphics::GetKeyValue(key);
    return true;
};


/*static*/ void CEnhancedGraphicsTab::SetItemValue(int32 index, float fValue)
{
    if (!IsFloatRow(index))
        return;

    // Mouse drags land on the same step grid as the pad so values stay tidy.
    int32 key = KeyOf(index);
    float lo = CEnhancedGraphics::GetKeyMin(key);
    float hi = CEnhancedGraphics::GetKeyMax(key);
    float step = s_aRow[index].m_fStep;
    if (step > 0.0f)
    {
        float fSteps = (fValue - lo) / step;
        fSteps = static_cast<float>(static_cast<int32>(fSteps + (fSteps >= 0.0f ? 0.5f : -0.5f)));
        fValue = lo + (fSteps * step);
    };
    fValue = Clamp(fValue, lo, hi);
    if (std::fabs(fValue - CEnhancedGraphics::GetKeyValue(key)) > 1e-5f)
        CEnhancedGraphics::SetKeyValue(key, fValue);
};


/*static*/ bool CEnhancedGraphicsTab::IsReadoutItem(int32 index)
{
    return ((index >= 0) && (index < ROW_NUM)) &&
           ((s_aRow[index].m_type == ROW_READ_WATER) || (s_aRow[index].m_type == ROW_READ_PERF));
};


/*static*/ const char* CEnhancedGraphicsTab::GetItemHeader(int32 index)
{
    return ((index >= 0) && (index < ROW_NUM)) ? s_aRow[index].m_pszHeader : nullptr;
};


/*static*/ void CEnhancedGraphicsTab::FormatItem(int32 index, char* pszBuffer, size_t capacity)
{
    if (capacity == 0)
        return;

    pszBuffer[0] = '\0';
    if ((index < 0) || (index >= ROW_NUM))
        return;

    // Label and value are separated by two or more spaces; a switch value is exactly
    // ON / OFF and an action has no value: both menu front ends rely on this.
    const ROWINFO& rRow = s_aRow[index];
    char szValue[64];
    szValue[0] = '\0';

    switch (rRow.m_type)
    {
    case ROW_MASTER:
        std::snprintf(szValue, sizeof(szValue), "%s", CEnhancedGraphics::IsEnhanced() ? "ON" : "OFF");
        break;

    case ROW_PRESET:
        {
            int32 nEdit = CEnhancedGraphics::GetOverrideCount();
            const char* pszName = CEnhancedGraphics::GetPresetName(CEnhancedGraphics::GetPreset());
            if (nEdit > 0)
                std::snprintf(szValue, sizeof(szValue), "%s + %d edit%s", pszName, nEdit, (nEdit == 1 ? "" : "s"));
            else
                std::snprintf(szValue, sizeof(szValue), "%s", pszName);
        }
        break;

    case ROW_KEY:
        {
            int32 key = KeyOf(index);
            if (key < 0)
                std::snprintf(szValue, sizeof(szValue), "n/a");
            else if (CEnhancedGraphics::IsKeyBool(key))
                std::snprintf(szValue, sizeof(szValue), "%s", (CEnhancedGraphics::GetKeyValue(key) >= 0.5f) ? "ON" : "OFF");
            else
                std::snprintf(szValue, sizeof(szValue), "%.2f%s", CEnhancedGraphics::GetKeyValue(key),
                              CEnhancedGraphics::IsKeyOverridden(key) ? " *" : "");
        }
        break;

    case ROW_READ_WATER:
        {
            int32 nMaterial = 0, nTriangle = 0;
            bool bBroken = false;
            CEnhancedGraphics::GetWaterStats(&nMaterial, &nTriangle, &bBroken);
            if (bBroken)
                std::snprintf(szValue, sizeof(szValue), "shaders unavailable");
            else if (nTriangle > 0)
                std::snprintf(szValue, sizeof(szValue), "%d material%s, %d tris", nMaterial, (nMaterial == 1 ? "" : "s"), nTriangle);
            else
                std::snprintf(szValue, sizeof(szValue), "none on this stage");
        }
        break;

    case ROW_READ_PERF:
        {
            float fFps = 0.0f, fPost = 0.0f, fWater = 0.0f;
            if (!CEnhancedGraphics::GetLastPerf(&fFps, &fPost, &fWater))
                std::snprintf(szValue, sizeof(szValue), "measuring...");
            else if (!CEnhancedGraphics::IsEnhanced())
                std::snprintf(szValue, sizeof(szValue), "%.0f fps (original)", fFps);
            else
                std::snprintf(szValue, sizeof(szValue), "%.0f fps  post %.2f  water %.2f ms", fFps, fPost, fWater);
        }
        break;

    default:    /* actions: no value */
        break;
    };

    if (szValue[0])
        std::snprintf(pszBuffer, capacity, "%-29s  %s", rRow.m_pszLabel, szValue);
    else
        std::snprintf(pszBuffer, capacity, "%s", rRow.m_pszLabel);
};


/*static*/ void CEnhancedGraphicsTab::GetItemHelp(int32 index, const char*& rpszScope, const char*& rpszDescription)
{
    if ((index < 0) || (index >= ROW_NUM))
        return;

    rpszScope = s_aRow[index].m_pszScope;
    rpszDescription = s_aRow[index].m_pszHelp;
};


/*static*/ void CEnhancedGraphicsTab::AdjustItem(int32 index, int32 direction, int32 multiplier)
{
    if ((index < 0) || (index >= ROW_NUM))
        return;

    const ROWINFO& rRow = s_aRow[index];
    switch (rRow.m_type)
    {
    case ROW_MASTER:
        CEnhancedGraphics::SetEnhanced(!CEnhancedGraphics::IsEnhanced());
        break;

    case ROW_PRESET:
        {
            int32 num = CEnhancedGraphics::PRESET_NUM;
            int32 step = (direction < 0) ? -1 : 1;
            int32 preset = ((static_cast<int32>(CEnhancedGraphics::GetPreset()) + step) % num + num) % num;
            CEnhancedGraphics::SetPreset(static_cast<CEnhancedGraphics::PRESET>(preset));
        }
        break;

    case ROW_KEY:
        {
            int32 key = KeyOf(index);
            if (key < 0)
                break;

            if (CEnhancedGraphics::IsKeyBool(key))
            {
                CEnhancedGraphics::SetKeyValue(key, (CEnhancedGraphics::GetKeyValue(key) >= 0.5f) ? 0.0f : 1.0f);
            }
            else
            {
                // Snap to the step grid so repeated presses never drift.
                float lo = CEnhancedGraphics::GetKeyMin(key);
                float fValue = CEnhancedGraphics::GetKeyValue(key) + (rRow.m_fStep * static_cast<float>(direction * multiplier));
                float fSteps = (fValue - lo) / rRow.m_fStep;
                fSteps = static_cast<float>(static_cast<int32>(fSteps + (fSteps >= 0.0f ? 0.5f : -0.5f)));
                CEnhancedGraphics::SetKeyValue(key, lo + (fSteps * rRow.m_fStep));
            };
        }
        break;

    default:    /* readouts and file actions do nothing on left / right */
        break;
    };
};


/*static*/ void CEnhancedGraphicsTab::ActivateItem(int32 index)
{
    if ((index < 0) || (index >= ROW_NUM))
        return;

    switch (s_aRow[index].m_type)
    {
    case ROW_SAVE:   CEnhancedGraphics::SaveIni();           break;
    case ROW_RELOAD: CEnhancedGraphics::ReloadIni();         break;
    case ROW_RESET:  CEnhancedGraphics::ClearAllOverrides(); break;
    case ROW_MASTER:
    case ROW_PRESET:
        AdjustItem(index, 1, 1);
        break;
    case ROW_KEY:
        if (!IsFloatRow(index))
            AdjustItem(index, 1, 1);
        break;
    default:
        break;
    };
};


/*static*/ void CEnhancedGraphicsTab::ResetItem(int32 index)
{
    if ((index < 0) || (index >= ROW_NUM))
        return;

    if (s_aRow[index].m_type == ROW_KEY)
        CEnhancedGraphics::ClearKeyOverride(KeyOf(index));
    else if (s_aRow[index].m_type == ROW_PRESET)
        CEnhancedGraphics::ClearAllOverrides();
    else if (s_aRow[index].m_type == ROW_MASTER)
        CEnhancedGraphics::SetEnhanced(true);
};


/*static*/ void CEnhancedGraphicsTab::ResetAll(void)
{
    CEnhancedGraphics::ClearAllOverrides();
};


#else /* defined(TARGET_PC) */


/*static*/ bool CEnhancedGraphicsTab::IsVisible(void) { return false; };
/*static*/ int32 CEnhancedGraphicsTab::GetItemNum(void) { return 0; };
/*static*/ bool CEnhancedGraphicsTab::IsNumericItem(int32) { return false; };
/*static*/ bool CEnhancedGraphicsTab::GetItemFraction(int32, float*) { return false; };
/*static*/ bool CEnhancedGraphicsTab::GetItemRange(int32, float*, float*, float*) { return false; };
/*static*/ void CEnhancedGraphicsTab::SetItemValue(int32, float) {};
/*static*/ bool CEnhancedGraphicsTab::IsReadoutItem(int32) { return false; };
/*static*/ const char* CEnhancedGraphicsTab::GetItemHeader(int32) { return nullptr; };
/*static*/ void CEnhancedGraphicsTab::FormatItem(int32, char* pszBuffer, size_t capacity) { if (capacity) pszBuffer[0] = '\0'; };
/*static*/ void CEnhancedGraphicsTab::GetItemHelp(int32, const char*&, const char*&) {};
/*static*/ void CEnhancedGraphicsTab::AdjustItem(int32, int32, int32) {};
/*static*/ void CEnhancedGraphicsTab::ActivateItem(int32) {};
/*static*/ void CEnhancedGraphicsTab::ResetItem(int32) {};
/*static*/ void CEnhancedGraphicsTab::ResetAll(void) {};


#endif /* defined(TARGET_PC) */
