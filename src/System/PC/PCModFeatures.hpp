#pragma once


class CPCModFeatures
{
public:
    static void Initialize(void);
    static bool IsIntroSkipEnabled(void);
    static bool IsMSAAEnabled(void);
    static bool IsBrutalDifficultiesEnabled(void);
    static bool IsWidescreenEnabled(void);

private:
    enum FLAG
    {
        FLAG_INTRO_SKIP          = (1 << 0),
        FLAG_MSAA                = (1 << 1),
        FLAG_BRUTAL_DIFFICULTIES = (1 << 2),
        FLAG_WIDESCREEN          = (1 << 3),
    };

    static unsigned int m_flags;
};
