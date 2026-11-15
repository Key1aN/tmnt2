#include "ToonManager.hpp"
#include "ModelToon.hpp"

#include "Game/System/Texture/TextureManager.hpp"

#include "rptoon.h"

#if defined(TARGET_PC)
#include "ToonGradient.hpp"
#include "System/PC/PCEnhancedGraphics.hpp"
#define TOON_ENHANCED_GRAPHICS
#endif /* defined(TARGET_PC) */


class CToonManagerImplement
{
public:
	static const int32 TEX_NAME_MAX = 16;

public:
    CToonManagerImplement(void);
    virtual ~CToonManagerImplement(void);
    void RegistModel(CModelToon* pModel);
    void RemoveMode(CModelToon* pModel);
    void SetTextureSet(const char* pszName);
    void RefreshEnhancement(void);

private:
#if defined(TOON_ENHANCED_GRAPHICS)
    struct ENHANCEDKEY
    {
        RwTexture* m_pSource;
        float      m_afValue[6];
    };

    bool BuildEnhancedGradient(int32 index, const ENHANCEDKEY& rKey);
    void RestoreOriginalGradients(void);
    void DestroyEnhancedGradients(void);

    RwTexture*   m_apGradientOriginal[CToonManager::PATTERN_MAX];
    RwTexture*   m_apGradientEnhanced[CToonManager::PATTERN_MAX];
    ENHANCEDKEY  m_aEnhancedKey[CToonManager::PATTERN_MAX];
    bool         m_abEnhancedValid[CToonManager::PATTERN_MAX];
    bool         m_bEnhancedReported;
#endif /* defined(TOON_ENHANCED_GRAPHICS) */

    char                  m_szTextureSetName[TEX_NAME_MAX];
    RpToonPaint*          m_apRpToonPaint[CToonManager::PATTERN_MAX];
    CToonManager::PATTERN m_patternCurrent;
    CList<CModelToon>     m_listModel;
    RpToonInk*            m_pRpToonInkSilhouette;
};


CToonManagerImplement::CToonManagerImplement(void)
: m_szTextureSetName()
, m_apRpToonPaint()
, m_patternCurrent(CToonManager::PATTERN_1)
, m_listModel()
, m_pRpToonInkSilhouette(nullptr)
{
#if defined(TOON_ENHANCED_GRAPHICS)
    std::memset(m_apGradientOriginal, 0, sizeof(m_apGradientOriginal));
    std::memset(m_apGradientEnhanced, 0, sizeof(m_apGradientEnhanced));
    std::memset(m_aEnhancedKey, 0, sizeof(m_aEnhancedKey));
    std::memset(m_abEnhancedValid, 0, sizeof(m_abEnhancedValid));
    m_bEnhancedReported = false;
#endif /* defined(TOON_ENHANCED_GRAPHICS) */

    m_pRpToonInkSilhouette = RpToonInkCreate();
    ASSERT(m_pRpToonInkSilhouette);
    
    if (m_pRpToonInkSilhouette)
    {
        RpToonInkSetOverallThickness(m_pRpToonInkSilhouette, 1.0f);
        RpToonInkSetColor(m_pRpToonInkSilhouette, { 0x00, 0xFF, 0xFF, 0xFF });
        RpToonInkSetName(m_pRpToonInkSilhouette, "silhouette");
    };

    for (int32 i = 0; i < COUNT_OF(m_apRpToonPaint); ++i)
    {
        m_apRpToonPaint[i] = RpToonPaintCreate();
        ASSERT(m_apRpToonPaint[i]);
    };
    
    m_szTextureSetName[0] = '\0';
};


CToonManagerImplement::~CToonManagerImplement(void)
{
	ASSERT(m_listModel.empty());

#if defined(TOON_ENHANCED_GRAPHICS)
    /* paints must point at the original gradients again before ours are destroyed */
    RestoreOriginalGradients();
    DestroyEnhancedGradients();
#endif /* defined(TOON_ENHANCED_GRAPHICS) */

	for (int32 i = 0; i < COUNT_OF(m_apRpToonPaint); ++i)
	{
		ASSERT(m_apRpToonPaint[i]);

		RpToonPaintDestroy(m_apRpToonPaint[i]);
		m_apRpToonPaint[i] = nullptr;
	};

	ASSERT(m_pRpToonInkSilhouette);
	
	RpToonInkDestroy(m_pRpToonInkSilhouette);
	m_pRpToonInkSilhouette = nullptr;
};


void CToonManagerImplement::RegistModel(CModelToon* pModel)
{
    m_listModel.push_back(pModel);

	CToonManager::PATTERN pattern = pModel->GetPattern();
    ASSERT(pattern >= CToonManager::PATTERN_START);
    ASSERT(pattern <  CToonManager::PATTERN_MAX);

	pModel->SetToonObject(m_pRpToonInkSilhouette, m_apRpToonPaint[pattern]);
};


void CToonManagerImplement::RemoveMode(CModelToon* pModel)
{
    ASSERT(m_listModel.contains(pModel));
    
    m_listModel.erase(pModel);
};


void CToonManagerImplement::SetTextureSet(const char* pszName)
{
    static const char* s_apszPatternTextureName[] =
    {
        "pat01",
        "pat02",
        "pat03",
        "pat04",
        "pat05",
	};

    ASSERT(std::strlen(pszName) < TEX_NAME_MAX);
	std::strcpy(m_szTextureSetName, pszName);
	
	CTextureManager::SetCurrentTextureSet(m_szTextureSetName);

    for (int32 i = 0; i < COUNT_OF(s_apszPatternTextureName); ++i)
    {
        RwTexture* pTexture = CTextureManager::GetRwTexture(s_apszPatternTextureName[i]);
        ASSERT(pTexture);

        RpToonPaintSetGradientTexture(m_apRpToonPaint[i], pTexture);

#if defined(TOON_ENHANCED_GRAPHICS)
        m_apGradientOriginal[i] = pTexture;
#endif /* defined(TOON_ENHANCED_GRAPHICS) */
    };

    RefreshEnhancement();
};


void CToonManagerImplement::RefreshEnhancement(void)
{
#if defined(TOON_ENHANCED_GRAPHICS)
    const EGSETTINGS& rSettings = CEnhancedGraphics::Get();
    if (!rSettings.m_bToon)
    {
        RestoreOriginalGradients();
        return;
    };

    float afTint[3];
    CEnhancedGraphics::GetToonShadowTint(afTint);

    int32 nEnhanced = 0;
    for (int32 i = 0; i < CToonManager::PATTERN_MAX; ++i)
    {
        if (!m_apGradientOriginal[i])
            continue;

        ENHANCEDKEY key;
        key.m_pSource = m_apGradientOriginal[i];
        key.m_afValue[0] = rSettings.m_fToonContrast;
        key.m_afValue[1] = rSettings.m_fToonShadowTint;
        key.m_afValue[2] = rSettings.m_fToonHighlight;
        key.m_afValue[3] = afTint[0];
        key.m_afValue[4] = afTint[1];
        key.m_afValue[5] = afTint[2];

        /* Rebuild only when the source or the parameters changed: a plain A/B toggle just swaps pointers. */
        bool bSame = m_abEnhancedValid[i] && m_apGradientEnhanced[i] &&
                     (std::memcmp(&key, &m_aEnhancedKey[i], sizeof(key)) == 0);

        if (!bSame && !BuildEnhancedGradient(i, key))
        {
            RpToonPaintSetGradientTexture(m_apRpToonPaint[i], m_apGradientOriginal[i]);
            continue;
        };

        RpToonPaintSetGradientTexture(m_apRpToonPaint[i], m_apGradientEnhanced[i]);
        ++nEnhanced;
    };

    if (!m_bEnhancedReported && m_szTextureSetName[0])
    {
        CEnhancedGraphics::Log("TOON gradients enhanced=%d of %d (texture set %s)", nEnhanced, int32(CToonManager::PATTERN_MAX), m_szTextureSetName);
        m_bEnhancedReported = true;
    };
#endif /* defined(TOON_ENHANCED_GRAPHICS) */
};


#if defined(TOON_ENHANCED_GRAPHICS)

bool CToonManagerImplement::BuildEnhancedGradient(int32 index, const ENHANCEDKEY& rKey)
{
    m_abEnhancedValid[index] = false;

    RwTexture* pSource = rKey.m_pSource;
    RwRaster* pRaster = (pSource ? RwTextureGetRaster(pSource) : nullptr);
    if (!pRaster)
        return false;

#if defined(TMNT2_RWDRV_D3D9)
    if (RwD3D9RasterIsCompressed(pRaster))
    {
        CEnhancedGraphics::Log("TOON %s: compressed raster, left original", RwTextureGetName(pSource));
        return false;
    };
#endif /* defined(TMNT2_RWDRV_D3D9) */

    const int32 w = RwRasterGetWidth(pRaster);
    const int32 h = RwRasterGetHeight(pRaster);
    if ((w <= 0) || (h <= 0) || ((w * h) > 4096))
    {
        CEnhancedGraphics::Log("TOON %s: unexpected size %dx%d, left original", RwTextureGetName(pSource), w, h);
        return false;
    };

    RwImage* pImage = RwImageCreate(w, h, 32);
    if (!pImage)
        return false;

    if (!RwImageAllocatePixels(pImage) || !RwImageSetFromRaster(pImage, pRaster))
    {
        RwImageDestroy(pImage);
        CEnhancedGraphics::Log("TOON %s: raster read failed, left original", RwTextureGetName(pSource));
        return false;
    };

    static uint8 s_aSrc[4096 * 4];
    static uint8 s_aDst[4096 * 4];
    const int32 stride = RwImageGetStride(pImage);
    uint8* pPixels = RwImageGetPixels(pImage);
    for (int32 y = 0; y < h; ++y)
        std::memcpy(&s_aSrc[y * w * 4], pPixels + (y * stride), static_cast<size_t>(w) * 4);

    ToonGradient::PARAMS params;
    params.m_fContrast   = rKey.m_afValue[0];
    params.m_fShadowTint = rKey.m_afValue[1];
    params.m_fHighlight  = rKey.m_afValue[2];
    params.m_afTint[0]   = rKey.m_afValue[3];
    params.m_afTint[1]   = rKey.m_afValue[4];
    params.m_afTint[2]   = rKey.m_afValue[5];
    ToonGradient::RESULT result = ToonGradient::Enhance(s_aSrc, s_aDst, w, h, params);

    for (int32 y = 0; y < h; ++y)
        std::memcpy(pPixels + (y * stride), &s_aDst[y * w * 4], static_cast<size_t>(w) * 4);

    RwTexture* pEnhanced = m_apGradientEnhanced[index];
    RwRaster* pEnhancedRaster = (pEnhanced ? RwTextureGetRaster(pEnhanced) : nullptr);
    if (pEnhancedRaster && ((RwRasterGetWidth(pEnhancedRaster) != w) || (RwRasterGetHeight(pEnhancedRaster) != h)))
    {
        RestoreOriginalGradients();
        RwTextureDestroy(pEnhanced);
        m_apGradientEnhanced[index] = pEnhanced = nullptr;
        pEnhancedRaster = nullptr;
    };

    if (!pEnhanced)
    {
        pEnhancedRaster = RwRasterCreate(w, h, 0, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        pEnhanced = (pEnhancedRaster ? RwTextureCreate(pEnhancedRaster) : nullptr);
        if (!pEnhanced)
        {
            if (pEnhancedRaster)
                RwRasterDestroy(pEnhancedRaster);
            RwImageDestroy(pImage);
            CEnhancedGraphics::Log("TOON %s: texture create failed, left original", RwTextureGetName(pSource));
            return false;
        };

        char szName[rwTEXTUREBASENAMELENGTH];
        std::snprintf(szName, sizeof(szName), "%.20s_eg", RwTextureGetName(pSource));
        RwTextureSetName(pEnhanced, szName);
        m_apGradientEnhanced[index] = pEnhanced;
    };

    RwTextureSetFilterMode(pEnhanced, RwTextureGetFilterMode(pSource));
    RwTextureSetAddressingU(pEnhanced, RwTextureGetAddressingU(pSource));
    RwTextureSetAddressingV(pEnhanced, RwTextureGetAddressingV(pSource));

    bool bOK = (RwRasterSetFromImage(pEnhancedRaster, pImage) != nullptr);
    RwImageDestroy(pImage);

    if (!bOK)
    {
        CEnhancedGraphics::Log("TOON %s: raster write failed, left original", RwTextureGetName(pSource));
        return false;
    };

    m_aEnhancedKey[index] = rKey;
    m_abEnhancedValid[index] = true;
    CEnhancedGraphics::Log("TOON %s %dx%d axis=%s dark_at_start=%d lum=%.2f..%.2f contrast=%.2f tint=%.2f highlight=%.2f",
                           RwTextureGetName(pSource), w, h, (result.m_iAxis == 0 ? "u" : "v"),
                           (result.m_bDarkAtStart ? 1 : 0), result.m_fLumMin, result.m_fLumMax,
                           params.m_fContrast, params.m_fShadowTint, params.m_fHighlight);
    return true;
};


void CToonManagerImplement::RestoreOriginalGradients(void)
{
    for (int32 i = 0; i < CToonManager::PATTERN_MAX; ++i)
    {
        if (m_apRpToonPaint[i] && m_apGradientOriginal[i] &&
            (RpToonPaintGetGradientTexture(m_apRpToonPaint[i]) != m_apGradientOriginal[i]))
        {
            RpToonPaintSetGradientTexture(m_apRpToonPaint[i], m_apGradientOriginal[i]);
        };
    };
};


void CToonManagerImplement::DestroyEnhancedGradients(void)
{
    for (int32 i = 0; i < CToonManager::PATTERN_MAX; ++i)
    {
        if (!m_apGradientEnhanced[i])
            continue;

        /* never destroy a texture a paint still references */
        if (m_apRpToonPaint[i] && (RpToonPaintGetGradientTexture(m_apRpToonPaint[i]) == m_apGradientEnhanced[i]))
            continue;

        RwTextureDestroy(m_apGradientEnhanced[i]);
        m_apGradientEnhanced[i] = nullptr;
        m_abEnhancedValid[i] = false;
    };
};

#endif /* defined(TOON_ENHANCED_GRAPHICS) */


static CToonManagerImplement* s_pToonManagerImpl = nullptr;


static inline CToonManagerImplement& ToonMan(void)
{
    ASSERT(s_pToonManagerImpl);
    return *s_pToonManagerImpl;
};


/*static*/ void CToonManager::Initialize(void)
{
    if (!s_pToonManagerImpl)
        s_pToonManagerImpl = new CToonManagerImplement;

#if defined(TOON_ENHANCED_GRAPHICS)
    CEnhancedGraphics::RegisterApplyCallback(&CToonManager::RefreshEnhancement);
#endif /* defined(TOON_ENHANCED_GRAPHICS) */
};


/*static*/ void CToonManager::Terminate(void)
{
    if (s_pToonManagerImpl)
    {
        delete s_pToonManagerImpl;
        s_pToonManagerImpl = nullptr;
    };
};


/*static*/ void CToonManager::RegistModel(CModelToon* pModel)
{
    ToonMan().RegistModel(pModel);
};


/*static*/ void CToonManager::RemoveModel(CModelToon* pModel)
{
    ToonMan().RemoveMode(pModel);
};


/*static*/ void CToonManager::SetTextureSet(const char* pszName)
{
    ToonMan().SetTextureSet(pszName);
};




/*static*/ void CToonManager::RefreshEnhancement(void)
{
    if (s_pToonManagerImpl)
        s_pToonManagerImpl->RefreshEnhancement();
};
