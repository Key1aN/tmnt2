#pragma once


class CPCModFeatures
{
public:
    static void Initialize(void);
    static bool IsIntroSkipEnabled(void);
    static bool IsMSAAEnabled(void);
    static bool IsMSAARuntimeAvailable(void);
    static bool IsBrutalDifficultiesEnabled(void);
    static bool IsWidescreenEnabled(void);
    static bool IsDebugToolsEnabled(void);
    static bool IsKaraiBossVariantEnabled(void);
    static bool IsComboAssistEnabled(void);

private:
    enum FLAG
    {
        FLAG_INTRO_SKIP          = (1 << 0),
        FLAG_MSAA                = (1 << 1),
        FLAG_BRUTAL_DIFFICULTIES = (1 << 2),
        FLAG_WIDESCREEN          = (1 << 3),
        FLAG_DEBUG_TOOLS         = (1 << 4),
        FLAG_KARAI_BOSS_VARIANT  = (1 << 5),
        FLAG_COMBO_ASSIST        = (1 << 6),
    };

    static unsigned int m_flags;
};
