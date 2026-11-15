#include "CircleShadowModule.hpp"

#include "Game/Component/Gimmick/Gimmick.hpp"
#include "Game/System/Character/Character.hpp"
#include "Game/System/Texture/TextureManager.hpp"
#include "Game/System/Map/WorldMap.hpp"
#include "Game/System/Misc/RenderStateStack.hpp"
#include "System/Common/RenderState.hpp"

#if defined(TARGET_PC)
#include "System/PC/PCEnhancedGraphics.hpp"
#define SHADOW_ENHANCED_GRAPHICS
#endif /* defined(TARGET_PC) */


#if defined(SHADOW_ENHANCED_GRAPHICS)
namespace
{
    RwTexture* s_pSoftShadowTexture = nullptr;
    bool s_bSoftShadowTried = false;


    /* 64x64 soft disc: smooth radial falloff to zero at the edge, cool dark colour. */
    RwTexture* CreateSoftShadowTexture(void)
    {
        const int32 SIZE = 64;
        RwImage* pImage = RwImageCreate(SIZE, SIZE, 32);
        if (!pImage)
            return nullptr;

        if (!RwImageAllocatePixels(pImage))
        {
            RwImageDestroy(pImage);
            return nullptr;
        };

        uint8* pPixels = RwImageGetPixels(pImage);
        const int32 stride = RwImageGetStride(pImage);
        for (int32 y = 0; y < SIZE; ++y)
        {
            for (int32 x = 0; x < SIZE; ++x)
            {
                float fx = ((static_cast<float>(x) + 0.5f) / static_cast<float>(SIZE)) * 2.0f - 1.0f;
                float fy = ((static_cast<float>(y) + 0.5f) / static_cast<float>(SIZE)) * 2.0f - 1.0f;
                float r = std::sqrt(fx * fx + fy * fy);
                float t = Clamp(r, 0.0f, 1.0f);
                float s = t * t * (3.0f - 2.0f * t);
                float a = std::pow(1.0f - s, 1.35f);

                uint8* p = pPixels + (y * stride) + (x * 4);
                p[0] = 18;
                p[1] = 20;
                p[2] = 32;
                p[3] = static_cast<uint8>(Clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
        };

        RwRaster* pRaster = RwRasterCreate(SIZE, SIZE, 0, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        RwTexture* pTexture = nullptr;
        if (pRaster && RwRasterSetFromImage(pRaster, pImage))
            pTexture = RwTextureCreate(pRaster);

        if (!pTexture && pRaster)
            RwRasterDestroy(pRaster);

        RwImageDestroy(pImage);

        if (pTexture)
        {
            RwTextureSetName(pTexture, "eg_soft_shadow");
            RwTextureSetFilterMode(pTexture, rwFILTERLINEAR);
            RwTextureSetAddressing(pTexture, rwTEXTUREADDRESSCLAMP);
        };

        CEnhancedGraphics::Log("SHADOW soft texture %s", (pTexture ? "created" : "FAILED (original shadows kept)"));
        return pTexture;
    };


    float SmoothStep01(float x)
    {
        float t = Clamp(x, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
};
#endif /* defined(SHADOW_ENHANCED_GRAPHICS) */


class CCircleShadowModuleForCharacter final : public CCircleShadowModule
{
public:
    CCircleShadowModuleForCharacter(CCharacter* pCharacter, float w, float h) : CCircleShadowModule(w, h), m_pCharacter(pCharacter) {};
    virtual ~CCircleShadowModuleForCharacter(void) {};
    virtual void GetPosition(RwV3d* pvPosition) const override { return m_pCharacter->GetBodyPosition(pvPosition); };
    virtual float GetDirection(void) const override { return m_pCharacter->GetDirection(); };

private:
    CCharacter* m_pCharacter;
};


class CCircleShadowModuleForGimmick final : public CCircleShadowModule
{
public:
    CCircleShadowModuleForGimmick(CGimmick* pGimmick, float w, float h) : CCircleShadowModule(w, h), m_pGimmick(pGimmick) {};
    virtual ~CCircleShadowModuleForGimmick(void) {};
    virtual void GetPosition(RwV3d* pvPosition) const override { return m_pGimmick->GetPosition(pvPosition); };
    virtual float GetDirection(void) const override { return 0.0f; };

private:
    CGimmick* m_pGimmick;
};


/*static*/ CCircleShadowModule* CCircleShadowModule::New(CGameObject* pGameObject, float w, float h, bool bEnable)
{
    ASSERT(pGameObject);

    CCircleShadowModule* pResult = nullptr;
    
    switch (pGameObject->GetType())
    {
    case GAMEOBJECTTYPE::CHARACTER:
        pResult = new CCircleShadowModuleForCharacter(static_cast<CCharacter*>(pGameObject), w, h);
        break;

    case GAMEOBJECTTYPE::GIMMICK:
        pResult = new CCircleShadowModuleForGimmick(static_cast<CGimmick*>(pGameObject), w, h);
        break;

    default:
        ASSERT(false);
        break;
    };

    if (pResult)
        pResult->SetEnable(bEnable);

    return pResult;
};


/*static*/ void CCircleShadowModule::DrawAll(void)
{
	CRenderStateStack RsStack;
	RsStack.Set(rwRENDERSTATEZWRITEENABLE, false);
	RsStack.Set(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA);
	RsStack.Set(rwRENDERSTATEDESTBLEND, rwBLENDINVSRCALPHA);
    RsStack.Push(rwRENDERSTATEVERTEXALPHAENABLE);

#if defined(SHADOW_ENHANCED_GRAPHICS)
    if (CEnhancedGraphics::Get().m_bShadow && !s_pSoftShadowTexture && !s_bSoftShadowTried)
    {
        s_bSoftShadowTried = true;
        s_pSoftShadowTexture = CreateSoftShadowTexture();
    };
#endif /* defined(SHADOW_ENHANCED_GRAPHICS) */

    for (CCircleShadowModule& it : m_listDraw)
        it.DrawNode();
};


/*static*/ CList<CCircleShadowModule> CCircleShadowModule::m_listDraw;


CCircleShadowModule::CCircleShadowModule(float w, float h)
: IModule(MODULETYPE::CIRCLE_SHADOW)
, m_pTexture(nullptr)
, m_bEnable(true)
, m_bDrawRequest(false)
, m_uSoftAlpha(0)
, m_uCoreAlpha(0)
, m_bEnhancedReady(false)
{
    RwMatrixSetIdentity(&m_matrix);
    RwMatrixSetIdentity(&m_matrixSoft);
    RwMatrixSetIdentity(&m_matrixCore);
    
    CTextureManager::SetCurrentTextureSet("common_ef");;
    m_pTexture = CTextureManager::GetRwTexture("Shadow1");
    ASSERT(m_pTexture);

    SetSize(w, h);

    m_nodeDraw.data = this;
    m_listDraw.push_back(&m_nodeDraw);
};


CCircleShadowModule::~CCircleShadowModule(void)
{
    ASSERT(!m_listDraw.empty());
    m_listDraw.erase(&m_nodeDraw);

#if defined(SHADOW_ENHANCED_GRAPHICS)
    /* the soft texture lives as long as any shadow does (one stage) */
    if (m_listDraw.empty())
    {
        if (s_pSoftShadowTexture)
        {
            RwTextureDestroy(s_pSoftShadowTexture);
            s_pSoftShadowTexture = nullptr;
        };
        s_bSoftShadowTried = false;
    };
#endif /* defined(SHADOW_ENHANCED_GRAPHICS) */
};


void CCircleShadowModule::Run(void)
{
    RwV3d vPosition = Math::VECTOR3_ZERO;
    GetPosition(&vPosition);

    float fHeight = CWorldMap::GetMapHeight(&vPosition);

    const CWorldMap::COLLISIONRESULT* pCollisionResult = CWorldMap::GetCollisionResult();
    ASSERT(pCollisionResult);

    if (!pCollisionResult)
        return;

    const float fMaxHeight = 5.0f;

    float fChrHeight = Clamp(vPosition.y - fHeight, 0.0f, fMaxHeight);
    float fT = (fChrHeight / fMaxHeight);

    float fScale = (1.0f - (0.5f * fT));
    uint8 uAlphaBasis = static_cast<uint8>(255.0f - (191.0f * fT));
	
    vPosition.y = fHeight + 0.05f;

    RwV3d vNormal = pCollisionResult->m_vNormal;
    RwV3d vAxisX = Math::VECTOR3_ZERO;
    RwV3d vAxisY = Math::VECTOR3_ZERO;
    RwV3d vAxisZ = Math::VECTOR3_ZERO;
    Math::Vec3_Cross(&vAxisX, &Math::VECTOR3_AXIS_Z, &vNormal);
    Math::Vec3_Cross(&vAxisZ, &vNormal, &Math::VECTOR3_AXIS_X);
    Math::Vec3_Normalize(&vAxisX, &vAxisX);
    Math::Vec3_Normalize(&vAxisY, &vNormal);
    Math::Vec3_Normalize(&vAxisZ, &vAxisZ);

    RwMatrix matShadow;
    RwMatrixSetIdentity(&matShadow);
    Math::Matrix_Update(&matShadow, &vAxisX, &vAxisY, &vAxisZ, &Math::VECTOR3_ZERO);

    RwMatrix matTranslation;
    RwMatrixSetIdentity(&matTranslation);
    Math::Matrix_Translate(&matTranslation, &vPosition);

    RwMatrix matScaling;
    RwMatrixSetIdentity(&matScaling);
    Math::Matrix_Scale(&matScaling, fScale, fScale, fScale);

    RwMatrix matRotate;
    RwMatrixSetIdentity(&matRotate);
    Math::Matrix_RotateY(&matRotate, GetDirection());

    RwMatrixSetIdentity(&m_matrix);
    Math::Matrix_Multiply(&m_matrix, &matScaling, &matRotate);
    Math::Matrix_Multiply(&m_matrix, &m_matrix, &matShadow);
    Math::Matrix_Multiply(&m_matrix, &m_matrix, &matTranslation);

    for (int32 i = 0; i < COUNT_OF(m_aVertices); ++i)
        RwIm3DVertexSetRGBA(&m_aVertices[i], 255, 255, 255, uAlphaBasis);

#if defined(SHADOW_ENHANCED_GRAPHICS)
    m_bEnhancedReady = false;
    if (CEnhancedGraphics::Get().m_bShadow)
        RunEnhanced(vPosition, fT, matShadow);
#endif /* defined(SHADOW_ENHANCED_GRAPHICS) */
};


void CCircleShadowModule::RunEnhanced(const RwV3d& rvGround, float fT, const RwMatrix& rmatBasis)
{
#if defined(SHADOW_ENHANCED_GRAPHICS)
    const EGSETTINGS& e = CEnhancedGraphics::Get();
    const float fMaxHeight = 5.0f;
    const float fHeightAbove = fT * fMaxHeight;

    /* Soft blob: widens and fades with height, but never disappears (landing spot stays readable). */
    float fSoftScale = e.m_fShadowSize * (1.0f + 0.30f * fT);
    float fSoftAlpha = e.m_fShadowOpacity * (1.0f - 0.75f * SmoothStep01(fT));

    /* Contact core: only near the ground, gone by about one unit of height. */
    float fCoreScale = e.m_fShadowSize * 0.55f;
    float fCoreAlpha = 0.0f;
    if (e.m_bContactShadow)
    {
        float k = 1.0f - SmoothStep01(fHeightAbove / 1.0f);
        fCoreAlpha = e.m_fContactOpacity * k * k;
    };

    m_uSoftAlpha = static_cast<uint8>(Clamp(fSoftAlpha, 0.0f, 1.0f) * 255.0f + 0.5f);
    m_uCoreAlpha = static_cast<uint8>(Clamp(fCoreAlpha, 0.0f, 1.0f) * 255.0f + 0.5f);

    /* Push the soft blob a little away from the light, along the ground. */
    RwV3d vTravel;
    CEnhancedGraphics::GetLightTravel(&vTravel);
    RwV3d vOffset = Math::VECTOR3_ZERO;
    float fLenXZ = std::sqrt(vTravel.x * vTravel.x + vTravel.z * vTravel.z);
    if (fLenXZ > 0.05f)
    {
        float fWidth = std::fabs(RwIm3DVertexGetPos(&m_aVertices[0])->x) * 2.0f;
        float fPush = e.m_fShadowSunOffset * fWidth * fSoftScale;
        vOffset.x = (vTravel.x / fLenXZ) * fPush;
        vOffset.z = (vTravel.z / fLenXZ) * fPush;
    };

    RwMatrix matRotate;
    RwMatrixSetIdentity(&matRotate);
    Math::Matrix_RotateY(&matRotate, GetDirection());

    RwV3d avPos[2] = { rvGround, rvGround };
    avPos[0].x += vOffset.x;
    avPos[0].z += vOffset.z;
    avPos[1].y += 0.01f;    /* core sits just above the soft blob */

    const float afScale[2] = { fSoftScale, fCoreScale };
    RwMatrix* apMatrix[2] = { &m_matrixSoft, &m_matrixCore };
    for (int32 i = 0; i < 2; ++i)
    {
        RwMatrix matScaling;
        RwMatrixSetIdentity(&matScaling);
        Math::Matrix_Scale(&matScaling, afScale[i], afScale[i], afScale[i]);

        RwMatrix matTranslation;
        RwMatrixSetIdentity(&matTranslation);
        Math::Matrix_Translate(&matTranslation, &avPos[i]);

        RwMatrixSetIdentity(apMatrix[i]);
        Math::Matrix_Multiply(apMatrix[i], &matScaling, &matRotate);
        Math::Matrix_Multiply(apMatrix[i], apMatrix[i], &rmatBasis);
        Math::Matrix_Multiply(apMatrix[i], apMatrix[i], &matTranslation);
    };

    m_bEnhancedReady = true;
#endif /* defined(SHADOW_ENHANCED_GRAPHICS) */
};


void CCircleShadowModule::DrawEnhanced(void)
{
#if defined(SHADOW_ENHANCED_GRAPHICS)
    /* Raster, filter and addressing are restored locally below, so RenderWare never keeps a
     * pointer to the soft-shadow raster (it is destroyed with the stage's last shadow). */
    void* pPrevRaster = nullptr;
    void* pPrevAddress = nullptr;
    void* pPrevFilter = nullptr;
    RwRenderStateGet(rwRENDERSTATETEXTURERASTER, &pPrevRaster);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RwTextureGetRaster(s_pSoftShadowTexture));
    RwRenderStateGet(rwRENDERSTATETEXTUREADDRESS, &pPrevAddress);
    RwRenderStateGet(rwRENDERSTATETEXTUREFILTER, &pPrevFilter);
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, reinterpret_cast<void*>(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, reinterpret_cast<void*>(rwFILTERLINEAR));

    const uint8 auAlpha[2] = { m_uSoftAlpha, m_uCoreAlpha };
    RwMatrix* apMatrix[2] = { &m_matrixSoft, &m_matrixCore };
    for (int32 i = 0; i < 2; ++i)
    {
        if (auAlpha[i] == 0)
            continue;

        for (int32 v = 0; v < COUNT_OF(m_aVertices); ++v)
            RwIm3DVertexSetRGBA(&m_aVertices[v], 255, 255, 255, auAlpha[i]);

        /* no rwIM3D_ALLOPAQUE: the vertex alpha carries the fade */
        if (RwIm3DTransform(m_aVertices, COUNT_OF(m_aVertices), apMatrix[i], rwIM3D_VERTEXUV))
        {
            RwIm3DRenderPrimitive(rwPRIMTYPETRISTRIP);
            RwIm3DEnd();
        };
    };

    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, pPrevAddress);
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, pPrevFilter);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, pPrevRaster);
#endif /* defined(SHADOW_ENHANCED_GRAPHICS) */
};


void CCircleShadowModule::Draw(void)
{
    m_bDrawRequest = true;
};


void CCircleShadowModule::SetSize(float w, float h)
{
    for (int32 i = 0; i < COUNT_OF(m_aVertices); ++i)
    {
        RwIm3DVertex* pVertex = &m_aVertices[i];

        float x = (2 % (i + 1) != 0 ? -1.0f : 1.0f) * (w * 0.5f);
        float y = 0.0f;
        float z = (2.0f * (i % 2 == 0) - 1) * (h * 0.5f);

        float u = (i / 2 ? 1.0f : 0.0f);
        float v = (i % 2 ? 0.0f : 1.0f);

        RwIm3DVertexSetPos(pVertex, x, y, z);
        RwIm3DVertexSetNormal(pVertex, 0.0f, 0.0f, 0.0f);
        RwIm3DVertexSetRGBA(pVertex, 255, 255, 255, 255);
        RwIm3DVertexSetU(pVertex, u);
        RwIm3DVertexSetV(pVertex, v);
    };
};


void CCircleShadowModule::SetEnable(bool bEnable)
{
    m_bEnable = bEnable;
};


bool CCircleShadowModule::IsEnabled(void) const
{
    return m_bEnable;
};


void CCircleShadowModule::DrawNode(void)
{
    if (!m_bEnable || !m_bDrawRequest)
        return;

#if defined(SHADOW_ENHANCED_GRAPHICS)
    if (m_bEnhancedReady && s_pSoftShadowTexture && CEnhancedGraphics::Get().m_bShadow)
    {
        DrawEnhanced();
        m_bDrawRequest = false;
        return;
    };
#endif /* defined(SHADOW_ENHANCED_GRAPHICS) */

    if (m_pTexture)
        RENDERSTATE_PUSH(rwRENDERSTATETEXTURERASTER, RwTextureGetRaster(m_pTexture));

    if (RwIm3DTransform(m_aVertices, COUNT_OF(m_aVertices), &m_matrix, rwIM3D_VERTEXUV | rwIM3D_ALLOPAQUE))
    {
        RwIm3DRenderPrimitive(rwPRIMTYPETRISTRIP);
        RwIm3DEnd();
    };

    m_bDrawRequest = false;
};