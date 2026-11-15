#include "WaterSurface.hpp"
#include "MapAttribute.hpp"
#include "WorldMap.hpp"

#if defined(TARGET_PC)
#include "System/PC/PCEnhancedGraphics.hpp"
#include "WaterSurfaceMesh.hpp"
#include "MapClumpModelMan.hpp"

#include "rpuvanim.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <direct.h>
#include <vector>


namespace
{
    const float HORIZONTAL_NY         = 0.95f;  /* |normal.y| for a horizontal triangle          */
    const float MIN_HORIZONTAL_FRAC   = 0.85f;  /* of a material's area                           */
    const float MIN_OVERLAP_UVANIM    = 0.25f;
    const float MIN_OVERLAP_STATIC    = 0.25f;  /* static texture: only its triangles over collision water are used */  /* ... when the material is UV-animated           */
    const float WATER_DY              = 1.0f;   /* render surface vs "water" collision height     */
    const float DOBON_DY              = 3.0f;   /* render surface vs "dobon" (deep water) floor   */
    const float GRID_CELL             = 4.0f;


    struct COLTRI
    {
        float m_ax, m_ay, m_az;
        float m_bx, m_by, m_bz;
        float m_cx, m_cy, m_cz;
        float m_fMaxDy;
    };


    struct MATSTAT
    {
        RpMaterial* m_pMaterial;
        int32       m_nTriangle;
        float       m_fArea;
        float       m_fHorizontalArea;
        float       m_fOverlapArea;
        float       m_fMinY;
        float       m_fMaxY;
        float       m_fAbsNySum;     /* area-weighted |normal.y| */
        int32       m_nHorzTri;
        float       m_afHorzBox[6];  /* min x,y,z  max x,y,z of horizontal triangles */
        bool        m_bUVAnim;
        bool        m_bKeyword;
        bool        m_bInclude;
        bool        m_bExclude;
        bool        m_bWater;
        bool        m_bClipToCollision; /* accepted on collision overlap alone: keep only wet triangles */
        bool        m_bClump;           /* from a map clump model, not the world */
        bool        m_bStrong;          /* named like water or UV-animated over collision water */
        float       m_fYOffset;         /* lift applied to this layer's overlay triangles */
        const char* m_pszModel;
    };


    struct CLUMPTRI
    {
        int32       m_matIndex;
        RwV3d       m_v[3];
    };


    struct BUILDCONTEXT
    {
        RpWorld*                    m_pWorld;
        std::vector<COLTRI>         m_aColTri;
        std::vector<int32>          m_aGridHead;     /* per cell: first index into m_aGridItem */
        std::vector<int32>          m_aGridItem;     /* (next, tri) pairs                       */
        float                       m_fMinX, m_fMinZ;
        int32                       m_nCellX, m_nCellZ;
        std::vector<MATSTAT>        m_aMat;
        size_t                      m_nWorldMaterial;
        std::vector<CLUMPTRI>       m_aClumpTri;
        WaterSurfaceMesh::INPUT     m_mesh;
        bool                        m_bNative;
        int32                       m_nSectorNoGeometry;
    };


    bool HasWaterKeyword(const char* pszName)
    {
        static const char* s_apszKey[] =
        {
            "water", "mizu", "wtr", "river", "kawa", "umi", "sea", "lake", "pond", "pool", "gesui",
        };

        if (!pszName || !pszName[0])
            return false;

        char szLower[64];
        size_t n = 0;
        for (; pszName[n] && (n < sizeof(szLower) - 1); ++n)
            szLower[n] = static_cast<char>(((pszName[n] >= 'A') && (pszName[n] <= 'Z')) ? (pszName[n] + 32) : pszName[n]);
        szLower[n] = '\0';

        for (int32 i = 0; i < COUNT_OF(s_apszKey); ++i)
        {
            if (std::strstr(szLower, s_apszKey[i]))
                return true;
        };

        return false;
    };


    const char* MaterialName(RpMaterial* pMaterial)
    {
        RwTexture* pTexture = (pMaterial ? RpMaterialGetTexture(pMaterial) : nullptr);
        return (pTexture ? RwTextureGetName(pTexture) : "");
    };


    void TriangleNormalArea(const RwV3d& a, const RwV3d& b, const RwV3d& c, float* pfNY, float* pfArea)
    {
        float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        float nx = uy * vz - uz * vy;
        float ny = uz * vx - ux * vz;
        float nz = ux * vy - uy * vx;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        *pfArea = 0.5f * len;
        *pfNY = (len > 1e-8f) ? (ny / len) : 0.0f;
    };


    /* height of the collision triangle under (x, z), if (x, z) is inside it in the XZ plane */
    bool HeightInTriangle(const COLTRI& t, float x, float z, float* pfY)
    {
        float d = (t.m_bz - t.m_cz) * (t.m_ax - t.m_cx) + (t.m_cx - t.m_bx) * (t.m_az - t.m_cz);
        if (std::fabs(d) < 1e-8f)
            return false;

        float l0 = ((t.m_bz - t.m_cz) * (x - t.m_cx) + (t.m_cx - t.m_bx) * (z - t.m_cz)) / d;
        float l1 = ((t.m_cz - t.m_az) * (x - t.m_cx) + (t.m_ax - t.m_cx) * (z - t.m_cz)) / d;
        float l2 = 1.0f - l0 - l1;
        const float eps = -0.02f;
        if ((l0 < eps) || (l1 < eps) || (l2 < eps))
            return false;

        *pfY = l0 * t.m_ay + l1 * t.m_by + l2 * t.m_cy;
        return true;
    };


    int32 CellIndex(const BUILDCONTEXT& ctx, float x, float z)
    {
        int32 cx = static_cast<int32>((x - ctx.m_fMinX) / GRID_CELL);
        int32 cz = static_cast<int32>((z - ctx.m_fMinZ) / GRID_CELL);
        if ((cx < 0) || (cz < 0) || (cx >= ctx.m_nCellX) || (cz >= ctx.m_nCellZ))
            return -1;
        return cz * ctx.m_nCellX + cx;
    };


    bool OverCollisionWater(const BUILDCONTEXT& ctx, float x, float y, float z)
    {
        int32 cell = CellIndex(ctx, x, z);
        if (cell < 0)
            return false;

        for (int32 it = ctx.m_aGridHead[cell]; it >= 0; it = ctx.m_aGridItem[it * 2])
        {
            const COLTRI& t = ctx.m_aColTri[ctx.m_aGridItem[it * 2 + 1]];
            float fy = 0.0f;
            if (HeightInTriangle(t, x, z, &fy) && (y >= (fy - 0.5f)) && (y <= (fy + t.m_fMaxDy)))
                return true;
        };

        return false;
    };


    /* centroid plus the three edge midpoints: robust for long thin triangles */
    float WetFraction(const BUILDCONTEXT& ctx, const RwV3d& a, const RwV3d& b, const RwV3d& c)
    {
        float px[4] = { (a.x + b.x + c.x) / 3.0f, (a.x + b.x) * 0.5f, (b.x + c.x) * 0.5f, (c.x + a.x) * 0.5f };
        float py[4] = { (a.y + b.y + c.y) / 3.0f, (a.y + b.y) * 0.5f, (b.y + c.y) * 0.5f, (c.y + a.y) * 0.5f };
        float pz[4] = { (a.z + b.z + c.z) / 3.0f, (a.z + b.z) * 0.5f, (b.z + c.z) * 0.5f, (c.z + a.z) * 0.5f };
        int32 nHit = 0;
        for (int32 k = 0; k < 4; ++k)
            nHit += (OverCollisionWater(ctx, px[k], py[k], pz[k]) ? 1 : 0);
        return static_cast<float>(nHit) * 0.25f;
    };


    /*---------------------------------------------------------------------
     *  collision footprint
     *-------------------------------------------------------------------*/

    RpWorldSector* CollectCollisionCallback(RpWorldSector* pSector, void* pData)
    {
        BUILDCONTEXT& ctx = *static_cast<BUILDCONTEXT*>(pData);
        RpWorld* pWorld = static_cast<RpWorld*>(ctx.m_pWorld);

        if (!pSector->triangles || !pSector->vertices)
            return pSector;

        const int32 nMaterial = RpWorldGetNumMaterials(pWorld);
        for (int32 i = 0; i < pSector->numTriangles; ++i)
        {
            const RpTriangle& tri = pSector->triangles[i];
            int32 matIndex = pSector->matListWindowBase + tri.matIndex;
            if ((matIndex < 0) || (matIndex >= nMaterial))
                continue;

            char szName[64];
            std::strncpy(szName, MaterialName(RpWorldGetMaterial(pWorld, matIndex)), sizeof(szName) - 1);
            szName[sizeof(szName) - 1] = '\0';

            MAPTYPES::ATTRIBUTE attribute = CMapAttribute::CheckAttribute(szName);
            if ((attribute != MAPTYPES::ATTRIBUTE_WATER) && (attribute != MAPTYPES::ATTRIBUTE_DOBON))
                continue;

            const RwV3d& a = pSector->vertices[tri.vertIndex[0]];
            const RwV3d& b = pSector->vertices[tri.vertIndex[1]];
            const RwV3d& c = pSector->vertices[tri.vertIndex[2]];

            COLTRI t;
            t.m_ax = a.x; t.m_ay = a.y; t.m_az = a.z;
            t.m_bx = b.x; t.m_by = b.y; t.m_bz = b.z;
            t.m_cx = c.x; t.m_cy = c.y; t.m_cz = c.z;
            t.m_fMaxDy = (attribute == MAPTYPES::ATTRIBUTE_DOBON) ? DOBON_DY : WATER_DY;
            ctx.m_aColTri.push_back(t);
        };

        return pSector;
    };


    void BuildGrid(BUILDCONTEXT& ctx)
    {
        ctx.m_nCellX = ctx.m_nCellZ = 0;
        if (ctx.m_aColTri.empty())
            return;

        float minX = 1e30f, minZ = 1e30f, maxX = -1e30f, maxZ = -1e30f;
        for (size_t i = 0; i < ctx.m_aColTri.size(); ++i)
        {
            const COLTRI& t = ctx.m_aColTri[i];
            float xs[3] = { t.m_ax, t.m_bx, t.m_cx };
            float zs[3] = { t.m_az, t.m_bz, t.m_cz };
            for (int32 k = 0; k < 3; ++k)
            {
                minX = (xs[k] < minX) ? xs[k] : minX; maxX = (xs[k] > maxX) ? xs[k] : maxX;
                minZ = (zs[k] < minZ) ? zs[k] : minZ; maxZ = (zs[k] > maxZ) ? zs[k] : maxZ;
            };
        };

        ctx.m_fMinX = minX - GRID_CELL;
        ctx.m_fMinZ = minZ - GRID_CELL;
        ctx.m_nCellX = static_cast<int32>((maxX - ctx.m_fMinX) / GRID_CELL) + 2;
        ctx.m_nCellZ = static_cast<int32>((maxZ - ctx.m_fMinZ) / GRID_CELL) + 2;
        if ((ctx.m_nCellX * ctx.m_nCellZ) > 4000000)
        {
            ctx.m_nCellX = ctx.m_nCellZ = 0;     /* absurd extents: give up on the footprint */
            return;
        };

        ctx.m_aGridHead.assign(static_cast<size_t>(ctx.m_nCellX * ctx.m_nCellZ), -1);
        ctx.m_aGridItem.clear();

        for (size_t i = 0; i < ctx.m_aColTri.size(); ++i)
        {
            const COLTRI& t = ctx.m_aColTri[i];
            float x0 = Min(t.m_ax, Min(t.m_bx, t.m_cx)), x1 = Max(t.m_ax, Max(t.m_bx, t.m_cx));
            float z0 = Min(t.m_az, Min(t.m_bz, t.m_cz)), z1 = Max(t.m_az, Max(t.m_bz, t.m_cz));
            int32 cx0 = static_cast<int32>((x0 - ctx.m_fMinX) / GRID_CELL), cx1 = static_cast<int32>((x1 - ctx.m_fMinX) / GRID_CELL);
            int32 cz0 = static_cast<int32>((z0 - ctx.m_fMinZ) / GRID_CELL), cz1 = static_cast<int32>((z1 - ctx.m_fMinZ) / GRID_CELL);
            for (int32 cz = cz0; cz <= cz1; ++cz)
            {
                for (int32 cx = cx0; cx <= cx1; ++cx)
                {
                    int32 cell = cz * ctx.m_nCellX + cx;
                    int32 item = static_cast<int32>(ctx.m_aGridItem.size() / 2);
                    ctx.m_aGridItem.push_back(ctx.m_aGridHead[cell]);
                    ctx.m_aGridItem.push_back(static_cast<int32>(i));
                    ctx.m_aGridHead[cell] = item;
                };
            };
        };
    };


    /*---------------------------------------------------------------------
     *  render materials
     *-------------------------------------------------------------------*/

    void ScoreTriangle(BUILDCONTEXT& ctx, int32 matIndex, const RwV3d& a, const RwV3d& b, const RwV3d& c)
    {
        if ((matIndex < 0) || (matIndex >= static_cast<int32>(ctx.m_aMat.size())))
            return;

        float ny = 0.0f, area = 0.0f;
        TriangleNormalArea(a, b, c, &ny, &area);

        MATSTAT& m = ctx.m_aMat[matIndex];
        if (m.m_nTriangle == 0)
        {
            m.m_fMinY = 1e30f;
            m.m_fMaxY = -1e30f;
        };
        ++m.m_nTriangle;
        m.m_fArea += area;
        m.m_fAbsNySum += std::fabs(ny) * area;
        const RwV3d* v3[3] = { &a, &b, &c };
        for (int32 k = 0; k < 3; ++k)
        {
            m.m_fMinY = (v3[k]->y < m.m_fMinY) ? v3[k]->y : m.m_fMinY;
            m.m_fMaxY = (v3[k]->y > m.m_fMaxY) ? v3[k]->y : m.m_fMaxY;
        };
        if (std::fabs(ny) < HORIZONTAL_NY)
            return;

        m.m_fHorizontalArea += area;
        for (int32 k = 0; k < 3; ++k)
        {
            float p3[3] = { v3[k]->x, v3[k]->y, v3[k]->z };
            for (int32 q = 0; q < 3; ++q)
            {
                if (((m.m_nHorzTri == 0) && (k == 0)) || (p3[q] < m.m_afHorzBox[q]))     m.m_afHorzBox[q] = p3[q];
                if (((m.m_nHorzTri == 0) && (k == 0)) || (p3[q] > m.m_afHorzBox[q + 3])) m.m_afHorzBox[q + 3] = p3[q];
            };
        };
        ++m.m_nHorzTri;

        m.m_fOverlapArea += area * WetFraction(ctx, a, b, c);
    };


    void CollectTriangle(BUILDCONTEXT& ctx, int32 matIndex, const RwV3d& a, const RwV3d& b, const RwV3d& c)
    {
        if ((matIndex < 0) || (matIndex >= static_cast<int32>(ctx.m_aMat.size())) || !ctx.m_aMat[matIndex].m_bWater)
            return;

        float ny = 0.0f, area = 0.0f;
        TriangleNormalArea(a, b, c, &ny, &area);
        if ((std::fabs(ny) < HORIZONTAL_NY) || (area < 1e-6f))
            return;   /* waterfalls and side faces keep their original look */

        if (ctx.m_aMat[matIndex].m_bClipToCollision && (WetFraction(ctx, a, b, c) < 0.5f))
            return;

        float dy = ctx.m_aMat[matIndex].m_fYOffset;
        WaterSurfaceMesh::AddTriangle(ctx.m_mesh, a.x, a.y + dy, a.z, b.x, b.y + dy, b.z, c.x, c.y + dy, c.z);
    };


    RpWorldSector* ScoreRenderCallback(RpWorldSector* pSector, void* pData)
    {
        BUILDCONTEXT& ctx = *static_cast<BUILDCONTEXT*>(pData);

        if (!pSector->triangles || !pSector->vertices)
        {
            if (pSector->numTriangles > 0)
                ++ctx.m_nSectorNoGeometry;
            return pSector;
        };

        for (int32 i = 0; i < pSector->numTriangles; ++i)
        {
            const RpTriangle& tri = pSector->triangles[i];
            ScoreTriangle(ctx, pSector->matListWindowBase + tri.matIndex,
                          pSector->vertices[tri.vertIndex[0]], pSector->vertices[tri.vertIndex[1]], pSector->vertices[tri.vertIndex[2]]);
        };

        return pSector;
    };


    RpWorldSector* CollectMeshCallback(RpWorldSector* pSector, void* pData)
    {
        BUILDCONTEXT& ctx = *static_cast<BUILDCONTEXT*>(pData);
        if (!pSector->triangles || !pSector->vertices)
            return pSector;

        for (int32 i = 0; i < pSector->numTriangles; ++i)
        {
            const RpTriangle& tri = pSector->triangles[i];
            CollectTriangle(ctx, pSector->matListWindowBase + tri.matIndex,
                            pSector->vertices[tri.vertIndex[0]], pSector->vertices[tri.vertIndex[1]], pSector->vertices[tri.vertIndex[2]]);
        };

        return pSector;
    };


    /*---------------------------------------------------------------------
     *  map clump models (v1.1.1)
     *
     *  Many stages draw their visible water as a separate alpha-blended,
     *  UV-animated clump (CMapClumpModelManager), not as world geometry:
     *  the Nexus arena's pool is one. Their triangles are brought into world
     *  space here and scored exactly like world triangles.
     *-------------------------------------------------------------------*/

    struct CLUMPPASS
    {
        BUILDCONTEXT*   m_pCtx;
        const char*     m_pszModel;
        int32           m_nAtomic;
        int32           m_nNoGeometry;
    };


    RpAtomic* ClumpAtomicCallback(RpAtomic* pAtomic, void* pData)
    {
        CLUMPPASS& pass = *static_cast<CLUMPPASS*>(pData);
        BUILDCONTEXT& ctx = *pass.m_pCtx;

        RpGeometry* pGeometry = RpAtomicGetGeometry(pAtomic);
        RwFrame* pFrame = RpAtomicGetFrame(pAtomic);
        if (!pGeometry)
            return pAtomic;

        ++pass.m_nAtomic;
        int32 nTri = RpGeometryGetNumTriangles(pGeometry);
        const RpTriangle* pTri = RpGeometryGetTriangles(pGeometry);
        RpMorphTarget* pMorph = (RpGeometryGetNumMorphTargets(pGeometry) > 0) ? RpGeometryGetMorphTarget(pGeometry, 0) : nullptr;
        const RwV3d* pVert = (pMorph ? RpMorphTargetGetVertices(pMorph) : nullptr);
        if (!pTri || !pVert || (nTri <= 0))
        {
            ++pass.m_nNoGeometry;
            return pAtomic;
        };

        const RwMatrix* pLTM = (pFrame ? RwFrameGetLTM(pFrame) : nullptr);
        RpMaterialList& matList = pGeometry->matList;
        int32 nGeoMat = RpGeometryGetNumMaterials(pGeometry);

        /* append (or find) this geometry's materials in the shared table */
        std::vector<int32> aMap(static_cast<size_t>(Max(nGeoMat, 0)), -1);
        for (int32 i = 0; i < nGeoMat; ++i)
        {
            RpMaterial* pMaterial = RpGeometryGetMaterial(pGeometry, i);
            for (size_t j = ctx.m_nWorldMaterial; j < ctx.m_aMat.size(); ++j)
            {
                if (ctx.m_aMat[j].m_pMaterial == pMaterial)
                {
                    aMap[i] = static_cast<int32>(j);
                    break;
                };
            };
            if (aMap[i] >= 0)
                continue;

            MATSTAT m;
            std::memset(&m, 0, sizeof(m));
            m.m_pMaterial = pMaterial;
            const char* pszName = MaterialName(pMaterial);
            m.m_bUVAnim = (pMaterial && RpMaterialUVAnimExists(pMaterial));
            m.m_bKeyword = HasWaterKeyword(pszName) || HasWaterKeyword(pass.m_pszModel);
            m.m_bInclude = CEnhancedGraphics::IsWaterTextureIncluded(pszName);
            m.m_bExclude = CEnhancedGraphics::IsWaterTextureExcluded(pszName);
            m.m_bClump = true;
            m.m_pszModel = pass.m_pszModel;
            aMap[i] = static_cast<int32>(ctx.m_aMat.size());
            ctx.m_aMat.push_back(m);
        };
        (void)matList;

        for (int32 i = 0; i < nTri; ++i)
        {
            const RpTriangle& tri = pTri[i];
            if (tri.matIndex >= static_cast<RwUInt16>(nGeoMat))
                continue;

            CLUMPTRI ct;
            ct.m_matIndex = aMap[tri.matIndex];
            for (int32 k = 0; k < 3; ++k)
            {
                ct.m_v[k] = pVert[tri.vertIndex[k]];
                if (pLTM)
                    RwV3dTransformPoint(&ct.m_v[k], &ct.m_v[k], pLTM);
            };
            ctx.m_aClumpTri.push_back(ct);
        };

        return pAtomic;
    };


    void CollectMapClump(RpClump* pClump, const char* pszName, void* pData)
    {
        CLUMPPASS pass;
        pass.m_pCtx = static_cast<BUILDCONTEXT*>(pData);
        pass.m_pszModel = pszName;
        pass.m_nAtomic = 0;
        pass.m_nNoGeometry = 0;
        RpClumpForAllAtomics(pClump, ClumpAtomicCallback, &pass);
        CEnhancedGraphics::Log("WATER map model %-16s atomics=%d without_triangles=%d", (pszName ? pszName : "?"),
                               pass.m_nAtomic, pass.m_nNoGeometry);
    };


    /* textures of this stage's water candidates, for ExportTextures (cleared with the stage) */
    std::vector<RwTexture*> s_apExportTexture;
    char s_szExportStage[32] = { 0 };


    bool WriteTga(const char* pszPath, RwRaster* pRaster)
    {
        int32 w = RwRasterGetWidth(pRaster), h = RwRasterGetHeight(pRaster);
        if ((w <= 0) || (h <= 0) || (w > 4096) || (h > 4096))
            return false;

        RwImage* pImage = RwImageCreate(w, h, 32);
        if (!pImage)
            return false;

        bool bOK = false;
        if (RwImageAllocatePixels(pImage) && RwImageSetFromRaster(pImage, pRaster))
        {
            FILE* pFile = std::fopen(pszPath, "wb");
            if (pFile)
            {
                uint8 hdr[18] = { 0 };
                hdr[2] = 2;                                   /* uncompressed true colour */
                hdr[12] = uint8(w & 0xFF); hdr[13] = uint8(w >> 8);
                hdr[14] = uint8(h & 0xFF); hdr[15] = uint8(h >> 8);
                hdr[16] = 32;
                hdr[17] = 0x28;                               /* top-left origin, 8 alpha bits */
                std::fwrite(hdr, 1, sizeof(hdr), pFile);
                const uint8* pPixels = RwImageGetPixels(pImage);
                int32 stride = RwImageGetStride(pImage);
                std::vector<uint8> row(static_cast<size_t>(w) * 4);
                for (int32 y = 0; y < h; ++y)
                {
                    const uint8* p = pPixels + (y * stride);
                    for (int32 x = 0; x < w; ++x)
                    {
                        row[x * 4 + 0] = p[x * 4 + 2];        /* RGBA -> BGRA */
                        row[x * 4 + 1] = p[x * 4 + 1];
                        row[x * 4 + 2] = p[x * 4 + 0];
                        row[x * 4 + 3] = p[x * 4 + 3];
                    };
                    std::fwrite(&row[0], 1, row.size(), pFile);
                };
                bOK = (std::fclose(pFile) == 0);
            };
        };

        RwImageDestroy(pImage);
        return bOK;
    };


    bool AverageTextureColor(RpMaterial* pMaterial, float afSum[3])
    {
        RwTexture* pTexture = (pMaterial ? RpMaterialGetTexture(pMaterial) : nullptr);
        RwRaster* pRaster = (pTexture ? RwTextureGetRaster(pTexture) : nullptr);
        if (!pRaster)
            return false;

#if defined(TMNT2_RWDRV_D3D9)
        if (RwD3D9RasterIsCompressed(pRaster))
            return false;   /* DXT: keep the default base colour rather than decode */
#endif /* defined(TMNT2_RWDRV_D3D9) */

        int32 w = RwRasterGetWidth(pRaster);
        int32 h = RwRasterGetHeight(pRaster);
        if ((w <= 0) || (h <= 0) || (w > 2048) || (h > 2048))
            return false;

        RwImage* pImage = RwImageCreate(w, h, 32);
        if (!pImage)
            return false;

        bool bOK = false;
        if (RwImageAllocatePixels(pImage) && RwImageSetFromRaster(pImage, pRaster))
        {
            const uint8* pPixels = RwImageGetPixels(pImage);
            int32 stride = RwImageGetStride(pImage);
            int32 stepX = Max(1, w / 64), stepY = Max(1, h / 64);
            double r = 0.0, g = 0.0, b = 0.0;
            int32 n = 0;
            for (int32 y = 0; y < h; y += stepY)
            {
                const uint8* pRow = pPixels + y * stride;
                for (int32 x = 0; x < w; x += stepX)
                {
                    r += pRow[x * 4 + 0];
                    g += pRow[x * 4 + 1];
                    b += pRow[x * 4 + 2];
                    ++n;
                };
            };

            if (n > 0)
            {
                afSum[0] = static_cast<float>(r / n) / 255.0f;
                afSum[1] = static_cast<float>(g / n) / 255.0f;
                afSum[2] = static_cast<float>(b / n) / 255.0f;
                bOK = true;
            };
        };

        RwImageDestroy(pImage);
        return bOK;
    };
};


namespace WaterSurface
{
    void Build(RpWorld* pWorld, RpWorld* pWorldCollision, const char* pszStageName)
    {
        CEnhancedGraphics::WaterClear();
        CEnhancedGraphics::SetWaterMaterialCount(0);
        s_apExportTexture.clear();
        std::snprintf(s_szExportStage, sizeof(s_szExportStage), "%s", (pszStageName ? pszStageName : "stage"));

        if (!CEnhancedGraphics::IsAvailable() || !pWorld)
            return;

        BUILDCONTEXT ctx;
        ctx.m_pWorld = pWorld;
        ctx.m_bNative = ((RpWorldGetFlags(pWorld) & rpWORLDNATIVE) != 0);
        ctx.m_nSectorNoGeometry = 0;
        ctx.m_fMinX = ctx.m_fMinZ = 0.0f;
        ctx.m_nCellX = ctx.m_nCellZ = 0;
        ctx.m_nWorldMaterial = 0;

        /* 1. collision footprint of "water" / "dobon" */
        if (pWorldCollision)
        {
            ctx.m_pWorld = pWorldCollision;
            RpWorldForAllWorldSectors(pWorldCollision, CollectCollisionCallback, &ctx);
            ctx.m_pWorld = pWorld;
        };
        BuildGrid(ctx);

        /* 2. score every render material */
        const int32 nMaterial = RpWorldGetNumMaterials(pWorld);
        ctx.m_aMat.resize(static_cast<size_t>(Max(nMaterial, 0)));
        for (int32 i = 0; i < nMaterial; ++i)
        {
            MATSTAT& m = ctx.m_aMat[i];
            std::memset(&m, 0, sizeof(m));
            m.m_pMaterial = RpWorldGetMaterial(pWorld, i);
            const char* pszName = MaterialName(m.m_pMaterial);
            m.m_bUVAnim = (m.m_pMaterial && RpMaterialUVAnimExists(m.m_pMaterial));
            m.m_bKeyword = HasWaterKeyword(pszName);
            m.m_bInclude = CEnhancedGraphics::IsWaterTextureIncluded(pszName);
            m.m_bExclude = CEnhancedGraphics::IsWaterTextureExcluded(pszName);
        };

        RpWorldForAllWorldSectors(pWorld, ScoreRenderCallback, &ctx);

        /* 2b. map clump models (separately drawn, often the visible water) */
        ctx.m_nWorldMaterial = ctx.m_aMat.size();
        CMapClumpModelManager::ForEachClump(CollectMapClump, &ctx);
        for (size_t i = 0; i < ctx.m_aClumpTri.size(); ++i)
        {
            const CLUMPTRI& t = ctx.m_aClumpTri[i];
            ScoreTriangle(ctx, t.m_matIndex, t.m_v[0], t.m_v[1], t.m_v[2]);
        };

        CEnhancedGraphics::Log("WATER stage=%s render_materials=%d native=%d sectors_without_geometry=%d collision_water_tris=%d",
                               (pszStageName ? pszStageName : "?"), nMaterial, (ctx.m_bNative ? 1 : 0),
                               ctx.m_nSectorNoGeometry, static_cast<int32>(ctx.m_aColTri.size()));
        {
            float bx[6] = { 1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f };
            for (size_t t = 0; t < ctx.m_aColTri.size(); ++t)
            {
                const COLTRI& c = ctx.m_aColTri[t];
                float p9[9] = { c.m_ax, c.m_ay, c.m_az, c.m_bx, c.m_by, c.m_bz, c.m_cx, c.m_cy, c.m_cz };
                for (int32 k = 0; k < 3; ++k)
                    for (int32 q = 0; q < 3; ++q)
                    {
                        bx[q] = (p9[k * 3 + q] < bx[q]) ? p9[k * 3 + q] : bx[q];
                        bx[q + 3] = (p9[k * 3 + q] > bx[q + 3]) ? p9[k * 3 + q] : bx[q + 3];
                    };
            };
            RwV3d vSurf = { 0.0f, 0.0f, 0.0f };
            CWorldMap::GetWaterSurfacePosition(&vSurf);
            CEnhancedGraphics::Log("WATER game water surface (for shadows/ripples) y=%.2f at ground 0", vSurf.y);
            if (!ctx.m_aColTri.empty())
                CEnhancedGraphics::Log("WATER collision water box=(%.1f %.1f %.1f)..(%.1f %.1f %.1f)", bx[0], bx[1], bx[2], bx[3], bx[4], bx[5]);
        };

        int32 nWaterMaterial = 0, nLogged = 0;
        float afColor[3] = { 0.0f, 0.0f, 0.0f };
        float fColorWeight = 0.0f;
        for (int32 i = 0; i < static_cast<int32>(ctx.m_aMat.size()); ++i)
        {
            MATSTAT& m = ctx.m_aMat[i];
            if (m.m_nTriangle == 0)
                continue;

            float fHorizontal = (m.m_fArea > 0.0f) ? (m.m_fHorizontalArea / m.m_fArea) : 0.0f;
            float fOverlap = (m.m_fHorizontalArea > 0.0f) ? (m.m_fOverlapArea / m.m_fHorizontalArea) : 0.0f;

            /* v1.1.1: a flat material with a static texture that lies over collision water is water
             * too (the Nexus arena's shallow pool, m60x_colosseum_floor1). Only the triangles that are
             * actually over collision water get the overlay, so a floor material that is partly dry
             * keeps its dry part untouched. */
            bool bNamedOrAnimated = m.m_bKeyword || (m.m_bUVAnim && (fOverlap >= MIN_OVERLAP_UVANIM));
            bool bStaticWet = (fOverlap >= MIN_OVERLAP_STATIC);
            bool bCandidate = (fHorizontal >= MIN_HORIZONTAL_FRAC) && (bNamedOrAnimated || bStaticWet);
            m.m_bWater = m.m_bInclude || (!m.m_bExclude && bCandidate);
            m.m_bClipToCollision = m.m_bWater && !m.m_bInclude && !bNamedOrAnimated;
            m.m_bStrong = m.m_bInclude || bNamedOrAnimated;

            if (m.m_bWater || m.m_bKeyword || m.m_bInclude || m.m_bUVAnim)
            {
                RwTexture* pTexture = (m.m_pMaterial ? RpMaterialGetTexture(m.m_pMaterial) : nullptr);
                bool bKnown = false;
                for (size_t k = 0; k < s_apExportTexture.size(); ++k)
                    bKnown = bKnown || (s_apExportTexture[k] == pTexture);
                if (pTexture && !bKnown)
                    s_apExportTexture.push_back(pTexture);
            };

            if ((m.m_bWater || m.m_bKeyword || m.m_bInclude || m.m_bExclude || (fOverlap > 0.05f) || m.m_bUVAnim) && (nLogged < 64))
            {
                CEnhancedGraphics::Log("WATER  material %3d %-24s%s%-10s tris=%5d area=%8.1f y=[%.2f..%.2f] avg_ny=%.2f horizontal=%3.0f%% over_collision_water=%3.0f%% uvanim=%d keyword=%d include=%d exclude=%d -> %s",
                                       i, MaterialName(m.m_pMaterial), (m.m_bClump ? " model=" : ""),
                                       (m.m_bClump && m.m_pszModel ? m.m_pszModel : ""), m.m_nTriangle, m.m_fArea, m.m_fMinY, m.m_fMaxY,
                                       (m.m_fArea > 0.0f) ? (m.m_fAbsNySum / m.m_fArea) : 0.0f,
                                       fHorizontal * 100.0f, fOverlap * 100.0f,
                                       (m.m_bUVAnim ? 1 : 0), (m.m_bKeyword ? 1 : 0), (m.m_bInclude ? 1 : 0), (m.m_bExclude ? 1 : 0),
                                       (m.m_bWater ? "WATER" : "-"));
                if (m.m_nHorzTri > 0)
                    CEnhancedGraphics::Log("WATER    flat part: tris=%d area=%.1f box=(%.1f %.1f %.1f)..(%.1f %.1f %.1f)",
                                           m.m_nHorzTri, m.m_fHorizontalArea,
                                           m.m_afHorzBox[0], m.m_afHorzBox[1], m.m_afHorzBox[2],
                                           m.m_afHorzBox[3], m.m_afHorzBox[4], m.m_afHorzBox[5]);
                ++nLogged;
            };

        };

        /* 2b'. a floor accepted only for lying over collision water is dropped when the stage
         * has real water (named or animated): m41n's tile floor 41tylu01 sits under the sewer's
         * animated water sheets and would otherwise get a second overlay. */
        bool bAnyStrong = false;
        for (size_t i = 0; i < ctx.m_aMat.size(); ++i)
            bAnyStrong = bAnyStrong || (ctx.m_aMat[i].m_bWater && ctx.m_aMat[i].m_bStrong);
        for (int32 i = 0; bAnyStrong && (i < static_cast<int32>(ctx.m_aMat.size())); ++i)
        {
            MATSTAT& m = ctx.m_aMat[i];
            if (m.m_bWater && !m.m_bStrong)
            {
                m.m_bWater = false;
                CEnhancedGraphics::Log("WATER  material %3d %s: wet floor under the stage's own water, skipped", i, MaterialName(m.m_pMaterial));
            };
        };

        /* 2c. stacked layers: stages draw one pool as several coplanar layers (Nexus: two
         * colosseum_water sheets + a ripple sheet over the pool floor). Overlaying each would
         * multiply the effect and z-fight, so per footprint only one layer is kept: a named /
         * animated one before a merely wet floor, then the larger one. It is lifted to the
         * highest layer of the group so it draws on top of all of them. */
        for (int32 i = 0; i < static_cast<int32>(ctx.m_aMat.size()); ++i)
        {
            MATSTAT& m = ctx.m_aMat[i];
            if (!m.m_bWater || (m.m_nHorzTri == 0))
                continue;

            int32 best = i;
            float topY = m.m_afHorzBox[4];
            for (int32 j = 0; j < static_cast<int32>(ctx.m_aMat.size()); ++j)
            {
                const MATSTAT& o = ctx.m_aMat[j];
                if ((j == i) || !o.m_bWater || (o.m_nHorzTri == 0))
                    continue;

                /* overlap of the two flat parts in x/z, relative to the smaller one */
                float ox = Min(m.m_afHorzBox[3], o.m_afHorzBox[3]) - Max(m.m_afHorzBox[0], o.m_afHorzBox[0]);
                float oz = Min(m.m_afHorzBox[5], o.m_afHorzBox[5]) - Max(m.m_afHorzBox[2], o.m_afHorzBox[2]);
                float dy = std::fabs(m.m_afHorzBox[4] - o.m_afHorzBox[4]);
                if ((ox <= 0.0f) || (oz <= 0.0f) || (dy > 1.5f))
                    continue;
                float am = (m.m_afHorzBox[3] - m.m_afHorzBox[0]) * (m.m_afHorzBox[5] - m.m_afHorzBox[2]);
                float ao = (o.m_afHorzBox[3] - o.m_afHorzBox[0]) * (o.m_afHorzBox[5] - o.m_afHorzBox[2]);
                if ((ox * oz) < (0.6f * Max(Min(am, ao), 1e-3f)))
                    continue;

                topY = Max(topY, o.m_afHorzBox[4]);
                const MATSTAT& b = ctx.m_aMat[best];
                bool bBetter = (o.m_bStrong != b.m_bStrong) ? o.m_bStrong :
                               (std::fabs(o.m_fHorizontalArea - b.m_fHorizontalArea) > 1e-3f) ? (o.m_fHorizontalArea > b.m_fHorizontalArea) :
                               (o.m_afHorzBox[4] > b.m_afHorzBox[4]) ? true :
                               ((o.m_afHorzBox[4] == b.m_afHorzBox[4]) && (j < best));
                if (bBetter)
                    best = j;
            };

            if (best != i)
            {
                m.m_bWater = false;
                CEnhancedGraphics::Log("WATER  material %3d %s: same pool as material %d, layer skipped", i, MaterialName(m.m_pMaterial), best);
            }
            else
            {
                m.m_fYOffset = Max(0.0f, topY - m.m_afHorzBox[4]) + 0.02f;
            };
        };

        for (int32 i = 0; i < static_cast<int32>(ctx.m_aMat.size()); ++i)
        {
            MATSTAT& m = ctx.m_aMat[i];
            if (!m.m_bWater)
                continue;

            ++nWaterMaterial;
            float afTex[3];
            if (AverageTextureColor(m.m_pMaterial, afTex))
            {
                float wgt = Max(m.m_fHorizontalArea, 0.001f);
                for (int32 k = 0; k < 3; ++k)
                    afColor[k] += afTex[k] * wgt;
                fColorWeight += wgt;
            };
        };

        CEnhancedGraphics::SetWaterMaterialCount(nWaterMaterial);

        /* 3. geometry from the chosen render triangles */
        WaterSurfaceMesh::Begin(ctx.m_mesh);
        if (nWaterMaterial > 0)
        {
            RpWorldForAllWorldSectors(pWorld, CollectMeshCallback, &ctx);
            for (size_t i = 0; i < ctx.m_aClumpTri.size(); ++i)
            {
                const CLUMPTRI& t = ctx.m_aClumpTri[i];
                CollectTriangle(ctx, t.m_matIndex, t.m_v[0], t.m_v[1], t.m_v[2]);
            };
        };

        const char* pszSource = "render";
        if ((WaterSurfaceMesh::TriangleCount(ctx.m_mesh) == 0) && CEnhancedGraphics::IsWaterCollisionFallbackEnabled() &&
            ((ctx.m_nSectorNoGeometry > 0) || ctx.m_bNative))
        {
            /* Opt-in only: render geometry is not readable, use horizontal collision water polygons. */
            for (size_t i = 0; i < ctx.m_aColTri.size(); ++i)
            {
                const COLTRI& t = ctx.m_aColTri[i];
                RwV3d a = { t.m_ax, t.m_ay, t.m_az }, b = { t.m_bx, t.m_by, t.m_bz }, c = { t.m_cx, t.m_cy, t.m_cz };
                float ny = 0.0f, area = 0.0f;
                TriangleNormalArea(a, b, c, &ny, &area);
                if ((t.m_fMaxDy == WATER_DY) && (std::fabs(ny) >= HORIZONTAL_NY) && (area > 1e-6f))
                    WaterSurfaceMesh::AddTriangle(ctx.m_mesh, a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z);
            };
            pszSource = "collision-fallback";
        };

        if (WaterSurfaceMesh::TriangleCount(ctx.m_mesh) == 0)
        {
            CEnhancedGraphics::Log("WATER none on this stage (water materials=%d)", nWaterMaterial);
            return;
        };

        WaterSurfaceMesh::OUTPUT out;
        WaterSurfaceMesh::Finish(ctx.m_mesh, out);

        RwRGBA base = { 60, 110, 120, 255 };
        if (fColorWeight > 0.0f)
        {
            base.red   = static_cast<RwUInt8>(Clamp(afColor[0] / fColorWeight, 0.0f, 1.0f) * 255.0f);
            base.green = static_cast<RwUInt8>(Clamp(afColor[1] / fColorWeight, 0.0f, 1.0f) * 255.0f);
            base.blue  = static_cast<RwUInt8>(Clamp(afColor[2] / fColorWeight, 0.0f, 1.0f) * 255.0f);
        };

        CEnhancedGraphics::Log("WATER source=%s input_tris=%d boundary_edges=%d subdivided_tris=%d vertices=%d capped=%d",
                               pszSource, WaterSurfaceMesh::TriangleCount(ctx.m_mesh), out.m_nBoundaryEdge,
                               static_cast<int32>(out.m_aIndex.size() / 3), static_cast<int32>(out.m_aVertex.size()),
                               (out.m_bCapped ? 1 : 0));

        std::vector<CEnhancedGraphics::WATERVERTEX> aVertex(out.m_aVertex.size());
        for (size_t i = 0; i < out.m_aVertex.size(); ++i)
        {
            const WaterSurfaceMesh::VERTEX& v = out.m_aVertex[i];
            CEnhancedGraphics::WATERVERTEX& w = aVertex[i];
            w.m_x = v.m_x; w.m_y = v.m_y; w.m_z = v.m_z;
            w.m_fShore = v.m_fShore;
            w.m_fDeep = v.m_fDeep;
            w.m_fPad0 = w.m_fPad1 = 0.0f;
        };

        CEnhancedGraphics::WaterSetMesh(&aVertex[0], static_cast<int32>(aVertex.size()),
                                        &out.m_aIndex[0], static_cast<int32>(out.m_aIndex.size()),
                                        base);
    };


    void Clear(void)
    {
        CEnhancedGraphics::WaterClear();
        s_apExportTexture.clear();
    };


    int32 ExportTextures(void)
    {
        _mkdir("TMNT2_water_textures");
        int32 nWritten = 0;
        for (size_t i = 0; i < s_apExportTexture.size(); ++i)
        {
            RwTexture* pTexture = s_apExportTexture[i];
            RwRaster* pRaster = RwTextureGetRaster(pTexture);
            if (!pRaster)
                continue;

            char szPath[260];
            const char* pszTex = RwTextureGetName(pTexture);
            size_t nStage = std::strlen(s_szExportStage);
            bool bPrefixed = (nStage > 0) && (std::strncmp(pszTex, s_szExportStage, nStage) == 0);
            if (bPrefixed)
                std::snprintf(szPath, sizeof(szPath), "TMNT2_water_textures\\%s.tga", pszTex);
            else
                std::snprintf(szPath, sizeof(szPath), "TMNT2_water_textures\\%s_%s.tga", s_szExportStage, pszTex);
#if defined(TMNT2_RWDRV_D3D9)
            if (RwD3D9RasterIsCompressed(pRaster))
            {
                CEnhancedGraphics::Log("WATER export %s: compressed (DXT) texture, skipped", RwTextureGetName(pTexture));
                continue;
            };
#endif /* defined(TMNT2_RWDRV_D3D9) */
            bool bOK = WriteTga(szPath, pRaster);
            CEnhancedGraphics::Log("WATER export %s -> %s", szPath, (bOK ? "ok" : "FAILED"));
            nWritten += (bOK ? 1 : 0);
        };
        return nWritten;
    };
};


#else /* defined(TARGET_PC) */


namespace WaterSurface
{
    void Build(RpWorld*, RpWorld*, const char*) {};
    void Clear(void) {};
    int32 ExportTextures(void) { return 0; };
};


#endif /* defined(TARGET_PC) */
