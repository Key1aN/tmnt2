#pragma once

#include "PCTypedefs.hpp"


class CPCSetting
{
public:
    static const PC::VIDEOMODE VIDEOMODE_DEFAULT;
    static PC::VIDEOMODE m_videomode;
    static bool m_bWindowMode;
    static int32 m_nMSAASamples;

public:
    static void Initialize(void);
    static void Terminate(void);
    static void Load(void);
    static void Save(void);
    static void SetMSAASamples(int32 nSamples);
    static void GetIniPath(std::string& Path);
};
