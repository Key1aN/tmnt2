#pragma once

#include <vector>


/*
 * Enhanced Graphics: pure mesh processing for the water overlay (no RenderWare).
 * Welds the selected water triangles, finds the open boundary (shoreline),
 * subdivides large triangles so the shore / depth fields interpolate well, and
 * writes per-vertex shore proximity and a depth proxy.
 */
namespace WaterSurfaceMesh
{
    struct VERTEX
    {
        float m_x, m_y, m_z;
        float m_fShore;     /* 1 on the boundary, 0 at m_fShoreWidth or further */
        float m_fDeep;      /* 0 on the boundary, 1 at m_fDeepWidth or further  */
    };

    struct INPUT
    {
        std::vector<float> m_afTri;     /* 9 floats per triangle */
    };

    struct OUTPUT
    {
        std::vector<VERTEX>         m_aVertex;
        std::vector<unsigned short> m_aIndex;
        int                         m_nBoundaryEdge;
        int                         m_nSubdivide;   /* largest per-triangle segment count used */
        bool                        m_bCapped;      /* triangles dropped to stay under 65535 vertices */
    };

    void Begin(INPUT& rIn);
    void AddTriangle(INPUT& rIn, float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz);
    int  TriangleCount(const INPUT& rIn);
    void Finish(const INPUT& rIn, OUTPUT& rOut,
                float fMaxEdge = 1.5f, float fShoreWidth = 1.2f, float fDeepWidth = 6.0f, int nMaxVertex = 60000);
};
