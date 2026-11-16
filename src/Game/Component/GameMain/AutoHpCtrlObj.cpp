#include "AutoHpCtrlObj.hpp"
#include "GameProperty.hpp"
#include "GamePlayer.hpp"

#if defined(TMNT2_DEBUG_TOOLS)
#include "Game/Component/TurtleCompanions/TurtleCompanions.hpp"
#endif /* defined(TMNT2_DEBUG_TOOLS) */


static const float s_afDamageSpeed[] =
{
    0.0f,
    -3.0f,
    -2.5f,
    -2.33f,
    -2.25f,
};


static const float s_afRecoverSpeed[] =
{
    0.0f,
    3.0f,
    2.5f,
    2.33f,
    2.25f,
};


CAutoHpCtrlObj::CAutoHpCtrlObj(MODE mode)
: CGameObject("AutoHpCtrl", GAMEOBJECTTYPE::GAMEPROPERTY)
, m_mode(mode)
, m_fSpeed(0.0f)
, m_fValue(0.0f)
{
    int32 nPlayerNum = CGameProperty::GetPlayerNum();
#if defined(TMNT2_DEBUG_TOOLS)
    // Turtle Companions: balance stays at the single-player value.
    nPlayerNum = CTurtleCompanions::BalancePlayerNum(nPlayerNum);
#endif /* defined(TMNT2_DEBUG_TOOLS) */

    if (m_mode == MODE_DAMAGE)
        m_fSpeed = s_afDamageSpeed[nPlayerNum];
    else
        m_fSpeed = s_afRecoverSpeed[nPlayerNum];
};


CAutoHpCtrlObj::~CAutoHpCtrlObj(void)
{
    ;
};


void CAutoHpCtrlObj::Run(void)
{
    m_fValue += CGameProperty::GetElapsedTime() * m_fSpeed;

    if (m_fValue >= 0.0f)
    {
		int32 nPlayerNum = CGameProperty::GetPlayerNum();
#if defined(TMNT2_DEBUG_TOOLS)
		// Turtle Companions: balance stays at the single-player value.
		nPlayerNum = CTurtleCompanions::BalancePlayerNum(nPlayerNum);
#endif /* defined(TMNT2_DEBUG_TOOLS) */
		for (int32 i = 0; i < nPlayerNum; ++i)
			CGameProperty::Player(i)->AddHP(static_cast<int32>(m_fValue));
    };

    m_fValue -= m_fValue;
};

