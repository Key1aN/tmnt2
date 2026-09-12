#include "PlayOptionData.hpp"

#if defined(TARGET_PC)
#include "System/PC/PCCrashReporter.hpp"
#include "System/PC/PCModFeatures.hpp"
#endif /* defined(TARGET_PC) */

#ifdef TARGET_WEB
#include "System/Web/WebSpecific.hpp"
#endif /* TARGET_WEB */


namespace
{
    GAMETYPES::DIFFICULTY GetDifficultyOptionCount(void)
    {
#if defined(TARGET_PC)
        if (CPCModFeatures::IsBrutalDifficultiesEnabled())
            return GAMETYPES::DIFFICULTY_OPTION_NUM;
#endif /* defined(TARGET_PC) */

        return GAMETYPES::DIFFICULTY_NUM;
    };
}; /* anonymous namespace */


CPlayOptionData::CPlayOptionData(void)
{
    ;
};


CPlayOptionData::~CPlayOptionData(void)
{
    ;
};


void CPlayOptionData::Initialize(void)
{
    SetDefault();    
};


void CPlayOptionData::Terminate(void)
{
    ;
};


void CPlayOptionData::SetDefault(void)
{
    m_difficulty = GAMETYPES::DIFFICULTY_NORMAL;
    m_bAutosaveFlag = true;
};


void CPlayOptionData::Apply(void)
{
	if (!IsValid())
	{
		OUTPUT("%s is invalid! Set to default!\n", __FUNCTION__);
		SetDefault();
	};

#if defined(TARGET_PC)
    if (CPCModFeatures::IsBrutalDifficultiesEnabled())
        CPCCrashReporter::Breadcrumb("DIFFICULTY option_apply raw=%d", static_cast<int32>(m_difficulty));
#endif /* defined(TARGET_PC) */
};


bool CPlayOptionData::IsValid(void) const
{
    const GAMETYPES::DIFFICULTY difficultyOptionCount = GetDifficultyOptionCount();

    ASSERT(m_difficulty >= GAMETYPES::DIFFICULTY_EASY);
    ASSERT(m_difficulty <  difficultyOptionCount);

    if ((m_difficulty <  GAMETYPES::DIFFICULTY_EASY) ||
        (m_difficulty >= difficultyOptionCount))
        return false;

    return true;
};


void CPlayOptionData::Snapshot(RAWDATA& rRawData) const
{
    rRawData.m_difficulty    = m_difficulty;
    rRawData.m_bAutosaveFlag = m_bAutosaveFlag;
};


void CPlayOptionData::Restore(const RAWDATA& rRawData)
{
    m_difficulty    = rRawData.m_difficulty;
    m_bAutosaveFlag = rRawData.m_bAutosaveFlag;
};


void CPlayOptionData::SetDifficulty(GAMETYPES::DIFFICULTY difficulty)
{
    const GAMETYPES::DIFFICULTY difficultyOptionCount = GetDifficultyOptionCount();

    ASSERT(difficulty >= GAMETYPES::DIFFICULTY_EASY);
    ASSERT(difficulty <  difficultyOptionCount);

#if defined(TARGET_PC)
    if (CPCModFeatures::IsBrutalDifficultiesEnabled() &&
        (m_difficulty != difficulty))
        CPCCrashReporter::Breadcrumb("DIFFICULTY option_set raw=%d", static_cast<int32>(difficulty));
#endif /* defined(TARGET_PC) */

    m_difficulty = difficulty;
};


GAMETYPES::DIFFICULTY CPlayOptionData::GetDifficulty(void) const
{
    return m_difficulty;
};


void CPlayOptionData::SetEnableAutosave(bool bSet)
{
    m_bAutosaveFlag = bSet;
};


bool CPlayOptionData::IsAutosaveEnabled(void) const
{
    return m_bAutosaveFlag;
};


bool CPlayOptionData::IsSimplifiedInput(void) const
{
#ifdef TARGET_WEB
    if (CWebSpecific::IsMobilePlatform())
        return true;
#endif /* TARGET_WEB */

    return false;
};
