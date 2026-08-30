#pragma once

#include "OptionTypes.hpp"


class CDisplayOptionData
{
public:
#ifdef TMNT2_FEATURE_DISPLAYRESO
    struct VIDEOMODE
    {
        char m_szName[64];
    };
#endif /* TMNT2_FEATURE_DISPLAYRESO */

    struct RAWDATA
    {
        bool m_bPlayerMarkerFlag;
        bool m_bFontEffectFlag;
        bool m_bHelpFlag;
#ifdef TMNT2_FEATURE_DISPLAYRESO
        int32 m_iVideomodeNo;
#endif /* TMNT2_FEATURE_DISPLAYRESO */
    };

public:
    CDisplayOptionData(void);
    ~CDisplayOptionData(void);
    void Initialize(void);
    void Terminate(void);
    void SetDefault(void);
    void Apply(void);
    bool IsValid(void) const;
    void Snapshot(RAWDATA& rRawData) const;
    void Restore(const RAWDATA& rRawData);
    void SetEnableHelp(bool bSet);
    void SetEnableFontEffect(bool bSet);
    void SetEnablePlayerMarker(bool bSet);
    bool IsHelpEnabled(void) const;
    bool IsEnabledFontEffect(void) const;
    bool IsEnabledPlayerMarker(void) const;
#ifdef TMNT2_FEATURE_DISPLAYRESO
    void SetVideomode(int32 No);
    bool ApplyVideomode(void);
    bool ApplyPCGraphics(bool bVideomodeChanged);
    int32 GetVideomodeNum(void) const;
    int32 GetVideomodeCur(void) const;
    const char* GetVideomodeName(int32 No) const;
    void SetMultiSamplingSamples(int32 nSamples);
    int32 GetMultiSamplingSamples(void) const;
    void SetAnisotropyLevel(int32 nLevel);
    int32 GetAnisotropyLevel(void) const;
#endif /* TMNT2_FEATURE_DISPLAYRESO */

private:
    bool m_bHelpFlag;
    bool m_bFontEffectFlag;
    bool m_bPlayerMarkerFlag;    
#ifdef TMNT2_FEATURE_DISPLAYRESO
    VIDEOMODE* m_pVideomode;
    int32 m_iVideomodeNum;
    int32 m_iVideomodeCur;
    int32 m_iMultiSamplingSamples;
    int32 m_iAnisotropyLevel;
#endif /* TMNT2_FEATURE_DISPLAYRESO */
};
