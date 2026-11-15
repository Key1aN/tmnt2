#pragma once

/*
 * TMNT2 Enhanced Graphics v1.1 (feature flag "enhanced-graphics").
 *
 * Runtime-switchable graphics improvements that keep the original cel-shaded
 * look: remapped toon gradients and a stage-aware light rig, soft contact
 * shadows, a stylized water overlay on classified water materials, and a
 * lightweight post chain (bloom, colour grade, optional FXAA-lite).
 *
 * Nothing here runs unless the mod manager wrote feature=enhanced-graphics.
 * With the feature active, F8 switches between ORIGINAL (every original code
 * path, unchanged) and ENHANCED. Shift+F8 cycles presets, Ctrl+F8 reloads
 * TMNT2_graphics.ini. Toggling never creates or destroys GPU resources.
 */


struct EGSETTINGS
{
    /* toon lighting (character gradients) */
    bool  m_bToon;
    float m_fToonContrast;      /* 0..1  narrower light/shadow transition */
    float m_fToonShadowTint;    /* 0..1  cool, stage-aware shadow colour   */
    float m_fToonHighlight;     /* 0..1  lifted top band                    */

    /* light rig (lit models: characters, gimmicks; the prelit world is untouched) */
    bool  m_bLightRig;
    float m_fKeyLight;          /* directional light scale                 */
    float m_fAmbientScale;      /* ambient light scale (< 1 = more contrast) */
    float m_fLightWarmth;       /* 0..1  warm key / cool fill               */

    /* blob shadows */
    bool  m_bShadow;
    float m_fShadowOpacity;
    float m_fShadowSize;
    bool  m_bContactShadow;
    float m_fContactOpacity;
    float m_fShadowSunOffset;   /* fraction of shadow size pushed away from the light */

    /* water overlay */
    bool  m_bWater;
    float m_fWaterStrength;
    float m_fWaterReflection;
    float m_fWaterSpecular;
    float m_fWaterFoam;
    float m_fWaterWaveScale;
    float m_fWaterWaveStrength;
    bool  m_bWaterDetail;

    /* post */
    bool  m_bBloom;
    float m_fBloomIntensity;
    float m_fBloomThreshold;
    bool  m_bGrade;
    float m_fContrast;
    float m_fSaturation;
    float m_fSplitTone;
    float m_fVignette;
    bool  m_bFXAA;

    /* textures */
    bool  m_bTrilinear;
};


class CEnhancedGraphics
{
public:
    enum PRESET
    {
        PRESET_CLASSIC_PLUS = 0,
        PRESET_ENHANCED,
        PRESET_HIGH,
        PRESET_CUSTOM,

        PRESET_NUM,
    };

    struct WATERVERTEX
    {
        float m_x, m_y, m_z;
        float m_fShore;     /* 1 at the edge of the water mesh, 0 away from it */
        float m_fDeep;      /* 0 near the edge .. 1 far from it                */
        float m_fPad0;
        float m_fPad1;
    };

    typedef void (*APPLYCALLBACK)(void);

    static void Initialize(void);
    static void Terminate(void);
    static void OnEngineStarted(void);
    static void OnEngineStopping(void);
    static void FrameBegin(void);

    static bool IsAvailable(void);
    static bool IsEnhanced(void);
    static PRESET GetPreset(void);
    static const char* GetPresetName(PRESET preset);

    /* Effective settings: every switch is OFF when the feature is unavailable or ORIGINAL is selected. */
    static const EGSETTINGS& Get(void);

    /* Called once whenever the effective settings change (toggle, preset, reload). */
    static void RegisterApplyCallback(APPLYCALLBACK pfnCallback);

    static void Log(const char* pszFormat, ...);

    /* water classification overrides from the INI */
    static bool IsWaterTextureIncluded(const char* pszTextureName);
    static bool IsWaterTextureExcluded(const char* pszTextureName);
    static bool IsWaterCollisionFallbackEnabled(void);

    /* stage context, set by the world map when a stage has loaded */
    static void SetStageContext(const RwRGBA& rFogColor, bool bFog, const RwV3d& rvLightTravel);
    static void GetToonShadowTint(float afRGB[3]);
    static void GetLightTravel(RwV3d* pvTravel);

    /* water overlay */
    static bool WaterSetMesh(const WATERVERTEX* pVertex, int32 nVertex,
                             const uint16* pIndex, int32 nIndex,
                             const RwRGBA& rBaseColor);
    static void WaterClear(void);
    static void WaterAdvanceTime(float dt);
    static void WaterDraw(void);

    /* post chain, called inside the 3D camera scene before radar / gauges */
    static void PostProcess(void);

    /* on-screen confirmation after a switch; returns false when nothing to show */
    static bool GetToast(char* pszBuffer, int32 nBufferSize, float* pfAlpha);

    /* ---- live editing (Debug Tools "Remaster" tab) ----
     * Keys are the TMNT2_graphics.ini [Overrides] keys. Values shown and edited are the
     * configured ones (preset + overrides), whether or not ENHANCED is currently on. */
    static void SetEnhanced(bool bEnhanced);
    static void SetPreset(PRESET preset);           /* clears all overrides */
    static int32 FindKey(const char* pszName);       /* -1 when unknown */
    static bool IsKeyBool(int32 key);
    static float GetKeyMin(int32 key);
    static float GetKeyMax(int32 key);
    static float GetKeyValue(int32 key);             /* bool keys: 0 / 1 */
    static void SetKeyValue(int32 key, float fValue); /* creates an override */
    static bool IsKeyOverridden(int32 key);
    static void ClearKeyOverride(int32 key);
    static void ClearAllOverrides(void);
    static int32 GetOverrideCount(void);
    static bool SaveIni(void);                       /* writes preset + overrides + water lists */
    static void ReloadIni(void);
    static void SetWaterMaterialCount(int32 nMaterial);
    static void GetWaterStats(int32* pnMaterial, int32* pnTriangle, bool* pbBroken);
    static bool GetLastPerf(float* pfFps, float* pfPostMs, float* pfWaterMs);
    static bool IsPostAvailable(void);
};
