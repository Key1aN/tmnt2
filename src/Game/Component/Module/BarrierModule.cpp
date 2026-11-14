#include "BarrierModule.hpp"

#include "Game/Component/Player/PlayerCharacter.hpp"
#include "Game/Component/GameMain/GameProperty.hpp"
#include "Game/Component/Effect/Tracer.hpp"
#include "Game/Component/Effect/EffectManager.hpp"
#include "Game/System/Character/Character.hpp"
#include "Game/System/GameObject/GameObject.hpp"
#include "Game/System/Texture/TextureManager.hpp"
#include "Game/System/Particle/Sphere.hpp"
#include "Game/System/Hit/HitCatchData.hpp"
#include "Game/System/Hit/HitAttackManager.hpp"
#include "System/Common/RenderState.hpp"



/*static*/ const char* CBarrierModule::BARRIER_MOTIONNAME = "Barrier";
/*static*/ CList<CBarrierModule> CBarrierModule::m_listDraw;


/*static*/ void CBarrierModule::DrawAll(void)
{
    for (CBarrierModule& it : m_listDraw)
        it.DrawBarrier();
};


CBarrierModule::CBarrierModule(CTracer* pTracer, CGameObject* pObject, float fRadius)
: IModule(MODULETYPE::BARRIER)
, m_pBarrierSphere(nullptr)
, m_pDummy(nullptr)
, m_pTracer(nullptr)
, m_pObject(nullptr)
, m_pTexture(nullptr)
, m_state(STATE_APPEAR)
, m_fRadius(0.0f)
, m_fScale(0.0f)
, m_bDisplay(false)
, m_target(CHitAttackData::TARGET_NONE)
, m_nPower(0)
, m_fTime(0.0f)
, m_fLivetime(0.0f)
#if defined(TMNT2_DEBUG_TOOLS)
, m_bShotReflectOnly(false)
, m_pTintTexture(nullptr)
, m_iShieldColorId(0)
#endif /* defined(TMNT2_DEBUG_TOOLS) */
{
    ASSERT(pTracer);
    ASSERT(pObject);

    m_fRadius = fRadius;
    m_pTracer = pTracer;
    m_pObject = pObject;
    m_pBarrierSphere = new CSphere(m_fRadius, 16, 16, 4, 16, { 0xFF, 0xFF, 0xFF, 0x80 });
    m_state = STATE_SLEEP;

    CTextureManager::SetCurrentTextureSet("common_ef");
    m_pTexture = CTextureManager::GetRwTexture("barrier");
    ASSERT(m_pTexture);
    m_pBarrierSphere->SetTexture(m_pTexture);

    m_pDummy = new CGameObject("Dummy", GAMEOBJECTTYPE::EFFECT);
    
    m_fTime = 0.0f;
    m_nPower = 5;
    m_fLivetime = 2.0f;
    m_target = CHitAttackData::TARGET_ENEMY_GIMMICK;

    m_nodeDraw.data = this;
    m_listDraw.push_back(&m_nodeDraw);
};


CBarrierModule::~CBarrierModule(void)
{
    ASSERT(!m_listDraw.empty());
    m_listDraw.erase(&m_nodeDraw);

#if defined(TMNT2_DEBUG_TOOLS)
    if (m_pTintTexture)
    {
        RwTextureDestroy(m_pTintTexture);
        m_pTintTexture = nullptr;
    };
#endif /* defined(TMNT2_DEBUG_TOOLS) */

    if (m_pBarrierSphere)
    {
        delete m_pBarrierSphere;
        m_pBarrierSphere = nullptr;
    };

    if (m_pDummy)
    {
        delete m_pDummy;
        m_pDummy = nullptr;
    };

    if (m_pTracer)
    {
        delete m_pTracer;
        m_pTracer = nullptr;
    };
};


void CBarrierModule::Run(void)
{
    m_fTime += CGameProperty::GetElapsedTime();

    switch (m_state)
    {
    case STATE_APPEAR:
        {
            m_fScale += (CGameProperty::GetElapsedTime() * 3.0f);
            if (m_fScale >= 1.0f)
            {
                m_fScale = 1.0f;
                m_state = STATE_IDLE;
                m_fTime = 0.0f;
            };
        }
        break;

    case STATE_IDLE:
        {
            if (m_fTime > m_fLivetime)
            {
                m_state = STATE_DISAPPEAR;
                m_fScale = 1.0f;
                m_bDisplay = true;
            };
        }
        break;

    case STATE_DISAPPEAR:
        {
            m_fScale -= CGameProperty::GetElapsedTime() * 3.0f;
            if (m_fScale <= 0.0f)
            {
                m_fTime = 0.0f;
                m_fScale = 0.0f;
                m_state = STATE_SLEEP;
                m_bDisplay = false;
            };
        }
        break;

    case STATE_SLEEP:
        break;

    default:
        ASSERT(false);
        break;
    };

    if (m_bDisplay)
    {
        RwV3d vPosition = Math::VECTOR3_ZERO;
        m_pTracer->GetPosition(&vPosition);
        
        RwV3d vScale = { m_fScale, m_fScale, m_fScale };
        m_pBarrierSphere->SetScale(&vScale);
        m_pBarrierSphere->SetPosition(&vPosition);

        RwSphere sphere = {};
        sphere.center = vPosition;
        sphere.radius = (m_fRadius * m_fScale);

#if defined(TMNT2_DEBUG_TOOLS)
        if (m_bShotReflectOnly)
        {
            // Shot-only attack that turns enemy projectiles around
            // (CShot reflects on this flag). No damage, no catch sphere.
            CHitAttackData hitAttack;
            hitAttack.SetObject(m_pObject->GetHandle());
            hitAttack.SetShape(CHitAttackData::SHAPE_SPHERE);
            hitAttack.SetSphere(&sphere);
            hitAttack.SetObjectPos(&vPosition);
            hitAttack.SetPower(0);
            hitAttack.SetTarget(CHitAttackData::TARGET_SHOT);
            hitAttack.SetAntiguard(CHitAttackData::ANTIGUARD_ENABLE);
            hitAttack.SetMotion("OvReflect");
            hitAttack.SetStatus(CHitAttackData::STATUS_KNOCK);
            hitAttack.SetFlagReflectShot(true);

            CHitAttackManager::RegistAttack(&hitAttack);

            m_pBarrierSphere->Run(CGameProperty::GetElapsedTime());
            return;
        };
#endif /* defined(TMNT2_DEBUG_TOOLS) */

        {
            CHitAttackData hitAttack;
            hitAttack.SetObject(m_pObject->GetHandle());
            hitAttack.SetShape(CHitAttackData::SHAPE_SPHERE);
            hitAttack.SetSphere(&sphere);
            hitAttack.SetObjectPos(&vPosition);
            hitAttack.SetPower(m_nPower);
            hitAttack.SetTarget(m_target);
            hitAttack.SetAntiguard(CHitAttackData::ANTIGUARD_INVALID);
            hitAttack.SetMotion(BARRIER_MOTIONNAME);
            hitAttack.SetStatus(CHitAttackData::STATUS_FLYAWAY);

            if (m_pObject->GetType() == GAMEOBJECTTYPE::CHARACTER)
            {
                CCharacter* pCharacter = static_cast<CCharacter*>(m_pObject);

                if (pCharacter->GetCharacterType() == CCharacter::TYPE_PLAYER)
                {
                    CPlayerCharacter* pPlayerCharacter = static_cast<CPlayerCharacter*>(pCharacter);
                    
                    if (pPlayerCharacter->GetID() == PLAYERID::ID_SPL)
                        hitAttack.SetFlyawayParameter(0.0f, 8.0f);
                };
            };

            CHitAttackManager::RegistAttack(&hitAttack);
        }

        {
            CHitCatchData hitCatch;
            hitCatch.SetObject(m_pDummy->GetHandle());
            hitCatch.SetObjectType(m_pDummy->GetType());
            hitCatch.SetShape(CHitCatchData::SHAPE_SPHERE);
            hitCatch.SetSphere(&sphere);
            
            CHitAttackManager::RegistCatch(&hitCatch);
        }

        m_pBarrierSphere->Run(CGameProperty::GetElapsedTime());
    };
};


void CBarrierModule::Draw(void)
{
    ;
};


void CBarrierModule::DrawBarrier(void)
{
    if (m_bDisplay)
    {
        RENDERSTATE_PUSH(rwRENDERSTATEZWRITEENABLE, false);

        m_pBarrierSphere->Draw();

        RENDERSTATE_POP(rwRENDERSTATEZWRITEENABLE);
    };
};


void CBarrierModule::SetLive(float fLivetime)
{
    m_fLivetime = fLivetime;
};


void CBarrierModule::SetHitTarget(CHitAttackData::TARGET target)
{
    m_target = target;
};


void CBarrierModule::SetStepOne(void)
{
    m_nPower = 10;
};


void CBarrierModule::SetStepTwo(void)
{
    m_nPower = 20;
};


void CBarrierModule::SetStepThree(void)
{
    m_nPower = 30;
};


void CBarrierModule::Appear(void)
{
    ASSERT(m_pBarrierSphere);
    ASSERT(m_pTracer);
    
    m_state = STATE_APPEAR;
    m_fScale = 0.0f;
    m_bDisplay = true;
    m_pBarrierSphere->ResetUV();

#if defined(TMNT2_DEBUG_TOOLS)
    // The guard shield comes up on every guard press: no start burst.
    if (m_bShotReflectOnly)
        return;
#endif /* defined(TMNT2_DEBUG_TOOLS) */

    RwV3d vPosition = Math::VECTOR3_ZERO;
    m_pTracer->GetPosition(&vPosition);

    CEffectManager::Play(EFFECTID::ID_BARRIER_START, &vPosition);
};


#if defined(TMNT2_DEBUG_TOOLS)
//
//  The barrier texture is green, so a vertex colour alone can only darken it.
//  This makes a copy with the same pattern and alpha in another hue: every
//  pixel keeps its brightness (strongest channel) and takes the tint colour.
//
static RwTexture* CreateTintedTexture(RwTexture* pSource, const RwRGBA& rTint)
{
    RwRaster* pSourceRaster = (pSource ? RwTextureGetRaster(pSource) : nullptr);
    if (!pSourceRaster)
        return nullptr;

    RwInt32 width = RwRasterGetWidth(pSourceRaster);
    RwInt32 height = RwRasterGetHeight(pSourceRaster);
    if ((width <= 0) || (height <= 0))
        return nullptr;

    RwImage* pImage = RwImageCreate(width, height, 32);
    if (!pImage)
        return nullptr;

    RwTexture* pResult = nullptr;

    if (RwImageAllocatePixels(pImage) && RwImageSetFromRaster(pImage, pSourceRaster))
    {
        RwUInt8* pPixels = RwImageGetPixels(pImage);
        RwInt32 stride = RwImageGetStride(pImage);

        for (RwInt32 y = 0; y < height; ++y)
        {
            RwUInt8* pRow = pPixels + (y * stride);
            for (RwInt32 x = 0; x < width; ++x)
            {
                // 32-bit RwImage pixels are RwRGBA: red, green, blue, alpha
                RwUInt8* pPixel = pRow + (x * 4);

                uint32 value = pPixel[0];
                if (pPixel[1] > value) value = pPixel[1];
                if (pPixel[2] > value) value = pPixel[2];

                pPixel[0] = static_cast<RwUInt8>((static_cast<uint32>(rTint.red)   * value) / 255);
                pPixel[1] = static_cast<RwUInt8>((static_cast<uint32>(rTint.green) * value) / 255);
                pPixel[2] = static_cast<RwUInt8>((static_cast<uint32>(rTint.blue)  * value) / 255);
            };
        };

        RwRaster* pRaster = RwRasterCreate(width, height, 0, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        if (pRaster)
        {
            if (RwRasterSetFromImage(pRaster, pImage))
                pResult = RwTextureCreate(pRaster);

            if (pResult)
            {
                RwTextureSetFilterMode(pResult, RwTextureGetFilterMode(pSource));
                RwTextureSetAddressingU(pResult, RwTextureGetAddressingU(pSource));
                RwTextureSetAddressingV(pResult, RwTextureGetAddressingV(pSource));
            }
            else
            {
                RwRasterDestroy(pRaster);
            };
        };
    };

    RwImageDestroy(pImage);

    return pResult;
};


void CBarrierModule::SetShotReflectOnly(const RwRGBA& rColor, int32 iColorId /*= 0*/)
{
    m_bShotReflectOnly = true;
    m_iShieldColorId = iColorId;

    if (m_pBarrierSphere)
    {
        delete m_pBarrierSphere;
        m_pBarrierSphere = nullptr;
    };

    // A second call recolours the shield: the old copy goes first.
    if (m_pTintTexture)
    {
        RwTextureDestroy(m_pTintTexture);
        m_pTintTexture = nullptr;
    };

    m_pTintTexture = CreateTintedTexture(m_pTexture, rColor);

    if (m_pTintTexture)
    {
        // The colour is in the texture now; the sphere itself is the same
        // white, half-transparent one the constructor builds.
        m_pBarrierSphere = new CSphere(m_fRadius, 16, 16, 4, 16, { 0xFF, 0xFF, 0xFF, rColor.alpha });
        m_pBarrierSphere->SetTexture(m_pTintTexture);
    }
    else
    {
        // The texture could not be copied on this device: a plain sphere in
        // the requested colour (no pattern) is still the right colour.
        m_pBarrierSphere = new CSphere(m_fRadius, 16, 16, 4, 16, rColor);
    };
};
#endif /* defined(TMNT2_DEBUG_TOOLS) */


void CBarrierModule::Disappear(void)
{
    if (m_state != STATE_SLEEP)
    {
        m_state = STATE_DISAPPEAR;
        m_fScale = 0.0f;
        m_bDisplay = true;
    };
};


CBarrierModule::STATE CBarrierModule::GetState(void) const
{
    return m_state;
};