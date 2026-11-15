#include "WaterSurfaceMesh.hpp"

#include <cmath>
#include <unordered_map>


namespace
{
    typedef unsigned long long KEY;

    const float WELD = 0.01f;


    KEY PositionKey(float x, float y, float z)
    {
        long long ix = static_cast<long long>(std::floor(x / WELD + 0.5f)) & 0x1FFFFF;
        long long iy = static_cast<long long>(std::floor(y / WELD + 0.5f)) & 0x1FFFFF;
        long long iz = static_cast<long long>(std::floor(z / WELD + 0.5f)) & 0x1FFFFF;
        return (static_cast<KEY>(ix) << 42) | (static_cast<KEY>(iy) << 21) | static_cast<KEY>(iz);
    };


    struct WELDER
    {
        std::unordered_map<KEY, int> m_map;
        std::vector<float>           m_aPos;   /* xyz */

        int Add(float x, float y, float z)
        {
            KEY key = PositionKey(x, y, z);
            std::unordered_map<KEY, int>::const_iterator it = m_map.find(key);
            if (it != m_map.end())
                return it->second;

            int index = static_cast<int>(m_aPos.size() / 3);
            m_aPos.push_back(x);
            m_aPos.push_back(y);
            m_aPos.push_back(z);
            m_map[key] = index;
            return index;
        };

        int Count(void) const { return static_cast<int>(m_aPos.size() / 3); };
    };


    float Length(const float* a, const float* b)
    {
        float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };


    int Segments(const float* a, const float* b, const float* c, float fMaxEdge)
    {
        float longest = Length(a, b);
        float l = Length(b, c); longest = (l > longest) ? l : longest;
        l = Length(c, a);       longest = (l > longest) ? l : longest;
        int n = static_cast<int>(std::ceil(longest / fMaxEdge));
        return (n < 1) ? 1 : ((n > 200) ? 200 : n);
    };


    float SmoothStep(float e0, float e1, float x)
    {
        float t = (x - e0) / (e1 - e0);
        t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
        return t * t * (3.0f - 2.0f * t);
    };


    float SegmentDistanceXZ(float px, float pz, const float* s)
    {
        float ax = s[0], az = s[1], bx = s[2], bz = s[3];
        float dx = bx - ax, dz = bz - az;
        float len2 = dx * dx + dz * dz;
        float t = (len2 > 1e-12f) ? (((px - ax) * dx + (pz - az) * dz) / len2) : 0.0f;
        t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
        float qx = ax + dx * t - px, qz = az + dz * t - pz;
        return std::sqrt(qx * qx + qz * qz);
    };
};


namespace WaterSurfaceMesh
{
    void Begin(INPUT& rIn)
    {
        rIn.m_afTri.clear();
    };


    void AddTriangle(INPUT& rIn, float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz)
    {
        const float v[9] = { ax, ay, az, bx, by, bz, cx, cy, cz };
        rIn.m_afTri.insert(rIn.m_afTri.end(), v, v + 9);
    };


    int TriangleCount(const INPUT& rIn)
    {
        return static_cast<int>(rIn.m_afTri.size() / 9);
    };


    void Finish(const INPUT& rIn, OUTPUT& rOut, float fMaxEdge, float fShoreWidth, float fDeepWidth, int nMaxVertex)
    {
        rOut.m_aVertex.clear();
        rOut.m_aIndex.clear();
        rOut.m_nBoundaryEdge = 0;
        rOut.m_nSubdivide = 1;
        rOut.m_bCapped = false;

        if (nMaxVertex > 65535)
            nMaxVertex = 65535;

        /* 1. weld the input */
        WELDER weld;
        std::vector<int> aTri;
        const int nIn = TriangleCount(rIn);
        for (int t = 0; t < nIn; ++t)
        {
            const float* p = &rIn.m_afTri[t * 9];
            int i0 = weld.Add(p[0], p[1], p[2]);
            int i1 = weld.Add(p[3], p[4], p[5]);
            int i2 = weld.Add(p[6], p[7], p[8]);
            if ((i0 == i1) || (i1 == i2) || (i2 == i0))
                continue;
            aTri.push_back(i0);
            aTri.push_back(i1);
            aTri.push_back(i2);
        };

        const int nTri = static_cast<int>(aTri.size() / 3);
        if (nTri == 0)
            return;

        /* 2. open edges (used by exactly one triangle) are the shoreline */
        std::unordered_map<KEY, int> edgeCount;
        for (int t = 0; t < nTri; ++t)
        {
            for (int e = 0; e < 3; ++e)
            {
                unsigned int a = static_cast<unsigned int>(aTri[t * 3 + e]);
                unsigned int b = static_cast<unsigned int>(aTri[t * 3 + ((e + 1) % 3)]);
                KEY key = (a < b) ? ((static_cast<KEY>(a) << 32) | b) : ((static_cast<KEY>(b) << 32) | a);
                ++edgeCount[key];
            };
        };

        std::vector<float> aSeg;    /* ax az bx bz */
        for (std::unordered_map<KEY, int>::const_iterator it = edgeCount.begin(); it != edgeCount.end(); ++it)
        {
            if (it->second != 1)
                continue;
            int a = static_cast<int>(it->first >> 32);
            int b = static_cast<int>(it->first & 0xFFFFFFFFu);
            aSeg.push_back(weld.m_aPos[a * 3 + 0]);
            aSeg.push_back(weld.m_aPos[a * 3 + 2]);
            aSeg.push_back(weld.m_aPos[b * 3 + 0]);
            aSeg.push_back(weld.m_aPos[b * 3 + 2]);
        };
        rOut.m_nBoundaryEdge = static_cast<int>(aSeg.size() / 4);

        /* 3. choose a subdivision that fits the vertex budget */
        float fEdge = fMaxEdge;
        for (int attempt = 0; attempt < 12; ++attempt)
        {
            long long estimate = 0;
            for (int t = 0; t < nTri; ++t)
            {
                int n = Segments(&weld.m_aPos[aTri[t * 3 + 0] * 3], &weld.m_aPos[aTri[t * 3 + 1] * 3], &weld.m_aPos[aTri[t * 3 + 2] * 3], fEdge);
                estimate += static_cast<long long>((n + 1) * (n + 2) / 2);
            };
            if (estimate <= nMaxVertex)
                break;
            fEdge *= 1.35f;
        };

        /* 4. tessellate (uniform barycentric grid per triangle, welded so shared edges meet) */
        WELDER outWeld;
        std::vector<int> aOutTri;
        for (int t = 0; t < nTri; ++t)
        {
            const float* a = &weld.m_aPos[aTri[t * 3 + 0] * 3];
            const float* b = &weld.m_aPos[aTri[t * 3 + 1] * 3];
            const float* c = &weld.m_aPos[aTri[t * 3 + 2] * 3];
            int n = Segments(a, b, c, fEdge);
            if (n > rOut.m_nSubdivide)
                rOut.m_nSubdivide = n;

            int startCount = outWeld.Count();
            size_t startTri = aOutTri.size();
            std::vector<int> aRow((n + 1) * (n + 2) / 2);
            int k = 0;
            for (int i = 0; i <= n; ++i)
            {
                for (int j = 0; j <= (n - i); ++j)
                {
                    float u = static_cast<float>(i) / static_cast<float>(n);
                    float v = static_cast<float>(j) / static_cast<float>(n);
                    float w = 1.0f - u - v;
                    aRow[k++] = outWeld.Add(a[0] * w + b[0] * u + c[0] * v,
                                            a[1] * w + b[1] * u + c[1] * v,
                                            a[2] * w + b[2] * u + c[2] * v);
                };
            };

            /* index of grid point (i, j) */
            for (int i = 0; i < n; ++i)
            {
                int rowStart = i * (n + 1) - (i * (i - 1)) / 2;
                int nextStart = (i + 1) * (n + 1) - ((i + 1) * i) / 2;
                for (int j = 0; j < (n - i); ++j)
                {
                    int p00 = aRow[rowStart + j];
                    int p01 = aRow[rowStart + j + 1];
                    int p10 = aRow[nextStart + j];
                    aOutTri.push_back(p00); aOutTri.push_back(p10); aOutTri.push_back(p01);
                    if (j < (n - i - 1))
                    {
                        int p11 = aRow[nextStart + j + 1];
                        aOutTri.push_back(p01); aOutTri.push_back(p10); aOutTri.push_back(p11);
                    };
                };
            };

            if (outWeld.Count() > nMaxVertex)
            {
                /* over budget even after coarsening: drop this triangle and stop */
                aOutTri.resize(startTri);
                outWeld.m_aPos.resize(static_cast<size_t>(startCount) * 3);
                rOut.m_bCapped = true;
                break;
            };
        };

        /* rebuild the map-free vertex list (positions past a rollback were removed) */
        const int nVertex = static_cast<int>(outWeld.m_aPos.size() / 3);
        rOut.m_aVertex.resize(nVertex);

        /* 5. shore / depth from the distance to the nearest open edge (XZ plane), via a grid */
        const float CELL = 2.0f;
        float minX = 1e30f, minZ = 1e30f, maxX = -1e30f, maxZ = -1e30f;
        for (size_t s = 0; s < aSeg.size(); s += 2)
        {
            minX = (aSeg[s] < minX) ? aSeg[s] : minX;         maxX = (aSeg[s] > maxX) ? aSeg[s] : maxX;
            minZ = (aSeg[s + 1] < minZ) ? aSeg[s + 1] : minZ; maxZ = (aSeg[s + 1] > maxZ) ? aSeg[s + 1] : maxZ;
        };

        const bool bHaveSeg = !aSeg.empty();
        int gx = 1, gz = 1;
        std::vector<std::vector<int> > aGrid;
        if (bHaveSeg)
        {
            gx = static_cast<int>((maxX - minX) / CELL) + 1;
            gz = static_cast<int>((maxZ - minZ) / CELL) + 1;
            if ((static_cast<long long>(gx) * gz) > 1000000)
            {
                float scale = std::sqrt(static_cast<float>(static_cast<long long>(gx) * gz) / 1000000.0f);
                gx = static_cast<int>(gx / scale) + 1;
                gz = static_cast<int>(gz / scale) + 1;
            };
            aGrid.resize(static_cast<size_t>(gx) * gz);
            const float cw = (maxX - minX) / gx + 1e-4f, ch = (maxZ - minZ) / gz + 1e-4f;
            for (int s = 0; s < static_cast<int>(aSeg.size() / 4); ++s)
            {
                const float* q = &aSeg[s * 4];
                int x0 = static_cast<int>(((q[0] < q[2] ? q[0] : q[2]) - minX) / cw);
                int x1 = static_cast<int>(((q[0] > q[2] ? q[0] : q[2]) - minX) / cw);
                int z0 = static_cast<int>(((q[1] < q[3] ? q[1] : q[3]) - minZ) / ch);
                int z1 = static_cast<int>(((q[1] > q[3] ? q[1] : q[3]) - minZ) / ch);
                x1 = (x1 >= gx) ? gx - 1 : x1; z1 = (z1 >= gz) ? gz - 1 : z1;
                for (int z = z0; z <= z1; ++z)
                    for (int x = x0; x <= x1; ++x)
                        aGrid[static_cast<size_t>(z) * gx + x].push_back(s);
            };

            for (int v = 0; v < nVertex; ++v)
            {
                float px = outWeld.m_aPos[v * 3 + 0], pz = outWeld.m_aPos[v * 3 + 2];
                float best = fDeepWidth;
                int cx = static_cast<int>((px - minX) / cw), cz = static_cast<int>((pz - minZ) / ch);
                int reach = static_cast<int>(std::ceil(fDeepWidth / ((cw < ch) ? cw : ch))) + 1;
                for (int r = 0; r <= reach; ++r)
                {
                    /* ring r around the cell; stop once the ring is farther than the best hit */
                    float ringDist = (static_cast<float>(r) - 1.0f) * ((cw < ch) ? cw : ch);
                    if ((r > 0) && (ringDist > best))
                        break;
                    for (int z = cz - r; z <= cz + r; ++z)
                    {
                        for (int x = cx - r; x <= cx + r; ++x)
                        {
                            if ((x < 0) || (z < 0) || (x >= gx) || (z >= gz))
                                continue;
                            if ((r > 0) && (x != cx - r) && (x != cx + r) && (z != cz - r) && (z != cz + r))
                                continue;
                            const std::vector<int>& cell = aGrid[static_cast<size_t>(z) * gx + x];
                            for (size_t i = 0; i < cell.size(); ++i)
                            {
                                float d = SegmentDistanceXZ(px, pz, &aSeg[cell[i] * 4]);
                                best = (d < best) ? d : best;
                            };
                        };
                    };
                };

                VERTEX& out = rOut.m_aVertex[v];
                out.m_fShore = 1.0f - SmoothStep(0.0f, fShoreWidth, best);
                out.m_fDeep = SmoothStep(0.0f, fDeepWidth, best);
            };
        }
        else
        {
            for (int v = 0; v < nVertex; ++v)
            {
                rOut.m_aVertex[v].m_fShore = 0.0f;
                rOut.m_aVertex[v].m_fDeep = 1.0f;
            };
        };

        for (int v = 0; v < nVertex; ++v)
        {
            rOut.m_aVertex[v].m_x = outWeld.m_aPos[v * 3 + 0];
            rOut.m_aVertex[v].m_y = outWeld.m_aPos[v * 3 + 1];
            rOut.m_aVertex[v].m_z = outWeld.m_aPos[v * 3 + 2];
        };

        rOut.m_aIndex.resize(aOutTri.size());
        for (size_t i = 0; i < aOutTri.size(); ++i)
            rOut.m_aIndex[i] = static_cast<unsigned short>(aOutTri[i]);
    };
};
