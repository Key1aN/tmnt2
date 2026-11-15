#include "ToonGradient.hpp"

#include <cmath>
#include <cstring>


namespace
{
    float Luma(const unsigned char* p)
    {
        return ((0.299f * p[0]) + (0.587f * p[1]) + (0.114f * p[2])) / 255.0f;
    };


    float Clamp01(float v)
    {
        return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
    };


    float SmoothStep(float e0, float e1, float x)
    {
        float t = Clamp01((x - e0) / (e1 - e0));
        return t * t * (3.0f - 2.0f * t);
    };


    const unsigned char* Texel(const unsigned char* pSrc, int width, int axis, int line, int index)
    {
        int x = (axis == 0) ? index : line;
        int y = (axis == 0) ? line : index;
        return pSrc + ((y * width) + x) * 4;
    };
};


namespace ToonGradient
{
    RESULT Enhance(const unsigned char* pSrc, unsigned char* pDst, int width, int height, const PARAMS& rParams)
    {
        RESULT result;
        result.m_iAxis = 0;
        result.m_bDarkAtStart = true;
        result.m_fLumMin = 0.0f;
        result.m_fLumMax = 1.0f;

        std::memcpy(pDst, pSrc, static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        if ((width <= 0) || (height <= 0) || ((width == 1) && (height == 1)))
            return result;

        /* The gradient runs along the axis whose averaged luminance varies most. */
        float afRange[2] = { 0.0f, 0.0f };
        for (int axis = 0; axis < 2; ++axis)
        {
            int length = (axis == 0) ? width : height;
            int lines  = (axis == 0) ? height : width;
            if (length < 2)
                continue;

            float lo = 1e9f, hi = -1e9f;
            for (int i = 0; i < length; ++i)
            {
                float sum = 0.0f;
                for (int j = 0; j < lines; ++j)
                    sum += Luma(Texel(pSrc, width, axis, j, i));
                float avg = sum / static_cast<float>(lines);
                lo = (avg < lo) ? avg : lo;
                hi = (avg > hi) ? avg : hi;
            };
            afRange[axis] = hi - lo;
        };

        const int axis   = (afRange[1] > afRange[0]) ? 1 : 0;
        const int length = (axis == 0) ? width : height;
        const int lines  = (axis == 0) ? height : width;
        result.m_iAxis = axis;

        /* luminance profile along the axis */
        float fFirst = 0.0f, fLast = 0.0f, fMin = 1e9f, fMax = -1e9f;
        for (int i = 0; i < length; ++i)
        {
            float sum = 0.0f;
            for (int j = 0; j < lines; ++j)
                sum += Luma(Texel(pSrc, width, axis, j, i));
            float avg = sum / static_cast<float>(lines);
            if (i == 0)
                fFirst = avg;
            if (i == (length - 1))
                fLast = avg;
            fMin = (avg < fMin) ? avg : fMin;
            fMax = (avg > fMax) ? avg : fMax;
        };

        const bool bDarkAtStart = (fFirst <= fLast);
        const float fRange = fMax - fMin;
        const bool bFlat = (fRange < 0.02f) || (length < 2);
        result.m_bDarkAtStart = bDarkAtStart;
        result.m_fLumMin = fMin;
        result.m_fLumMax = fMax;

        const float k = 1.0f + 0.35f * Clamp01(rParams.m_fContrast);
        const bool bNearest = (length <= 16);   /* keep hard toon steps hard */

        for (int j = 0; j < lines; ++j)
        {
            for (int i = 0; i < length; ++i)
            {
                float s = (length > 1) ? (static_cast<float>(i) / static_cast<float>(length - 1)) : 0.0f;
                if (!bDarkAtStart)
                    s = 1.0f - s;

                float c[4];
                if (bFlat)
                {
                    const unsigned char* p = Texel(pSrc, width, axis, j, i);
                    for (int ch = 0; ch < 4; ++ch)
                        c[ch] = static_cast<float>(p[ch]) / 255.0f;
                }
                else
                {
                    /* tighten the transition around the middle of the ramp */
                    float sw = Clamp01(0.5f + (s - 0.5f) * k);
                    float idx = (bDarkAtStart ? sw : (1.0f - sw)) * static_cast<float>(length - 1);

                    if (bNearest)
                    {
                        int n = static_cast<int>(std::floor(idx + 0.5f));
                        const unsigned char* p = Texel(pSrc, width, axis, j, n);
                        for (int ch = 0; ch < 4; ++ch)
                            c[ch] = static_cast<float>(p[ch]) / 255.0f;
                    }
                    else
                    {
                        int n0 = static_cast<int>(std::floor(idx));
                        int n1 = (n0 + 1 < length) ? (n0 + 1) : n0;
                        float f = idx - static_cast<float>(n0);
                        const unsigned char* p0 = Texel(pSrc, width, axis, j, n0);
                        const unsigned char* p1 = Texel(pSrc, width, axis, j, n1);
                        for (int ch = 0; ch < 4; ++ch)
                            c[ch] = ((static_cast<float>(p0[ch]) * (1.0f - f)) + (static_cast<float>(p1[ch]) * f)) / 255.0f;
                    };
                };

                float lum = (0.299f * c[0]) + (0.587f * c[1]) + (0.114f * c[2]);
                float dark = bFlat ? (1.0f - s) : Clamp01((fMax - lum) / fRange);

                for (int ch = 0; ch < 3; ++ch)
                {
                    float tint = 1.0f + (rParams.m_afTint[ch] - 1.0f) * (dark * Clamp01(rParams.m_fShadowTint));
                    c[ch] *= tint;
                    c[ch] *= 1.0f - (0.10f * Clamp01(rParams.m_fContrast) * dark);
                };

                /* Highlight follows how lit the texel is (not its position), so hard toon steps stay hard. */
                float lit = bFlat ? s : (1.0f - dark);
                float hl = SmoothStep(0.80f, 0.97f, lit) * Clamp01(rParams.m_fHighlight) * 0.22f;
                for (int ch = 0; ch < 3; ++ch)
                    c[ch] += (1.0f - Clamp01(c[ch])) * hl;

                unsigned char* q = pDst + ((((axis == 0) ? j : i) * width) + ((axis == 0) ? i : j)) * 4;
                for (int ch = 0; ch < 4; ++ch)
                    q[ch] = static_cast<unsigned char>(Clamp01(c[ch]) * 255.0f + 0.5f);
            };
        };

        return result;
    };
};
