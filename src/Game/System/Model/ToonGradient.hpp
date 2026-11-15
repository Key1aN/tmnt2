#pragma once


/*
 * Enhanced Graphics: remap of the RpToon shading gradients (pat01..pat05).
 *
 * The RenderWare toon shader looks the lighting term up in these small
 * gradient textures, so remapping them changes character shading everywhere
 * without touching RenderWare's precompiled toon shaders or the ink outlines.
 *
 * Pure function on RGBA8 pixels; no RenderWare calls (unit-tested off-target).
 */
namespace ToonGradient
{
    struct PARAMS
    {
        float m_fContrast;      /* 0..1 narrower light/shadow transition     */
        float m_fShadowTint;    /* 0..1 strength of the hue tint in shadows   */
        float m_afTint[3];      /* luminance-normalised shadow hue            */
        float m_fHighlight;     /* 0..1 lift of the mid band (wider lit area) */

        /* v1.1.1 light rig. TMNT2's cel shader ignores the colour of the stage lights
         * (only their direction picks the ramp texel) and the world is prelit, so the
         * "light" controls act here, on the ramp. All neutral when m_bLightRig is false. */
        bool  m_bLightRig;
        float m_fAmbient;       /* 0.5..1.2 brightness of the shadow band, 1 = original */
        float m_fKey;           /* 0.5..1.5 lit band: < 1 dims it, > 1 widens it        */
        float m_fWarmth;        /* 0..1 warm lit side / cool shadow side                */
    };

    struct RESULT
    {
        int   m_iAxis;          /* 0 = along width, 1 = along height          */
        bool  m_bDarkAtStart;
        float m_fLumMin;
        float m_fLumMax;
    };

    /* src/dst: width*height RGBA8 texels, row-major, r g b a byte order. */
    RESULT Enhance(const unsigned char* pSrc, unsigned char* pDst, int width, int height, const PARAMS& rParams);
};
