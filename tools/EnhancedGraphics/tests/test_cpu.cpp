// Off-target unit tests for the pure Enhanced Graphics code (g++ on Linux).
#include "Game/System/Model/ToonGradient.hpp"
#include "Game/System/Map/WaterSurfaceMesh.hpp"
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <cstring>

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static float L(const unsigned char* p) { return (0.299f*p[0]+0.587f*p[1]+0.114f*p[2])/255.0f; }

static void TestGradient(int w, int h, bool darkFirst, bool stepped, const char* name)
{
    int axisLen = (w >= h) ? w : h;
    std::vector<unsigned char> src(w*h*4), dst(w*h*4);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        int i = (w >= h) ? x : y;
        float s = (axisLen > 1) ? float(i)/(axisLen-1) : 0.f;
        if (!darkFirst) s = 1.f - s;
        float v = stepped ? (s < 0.5f ? 0.45f : 1.0f) : (0.35f + 0.65f*s);
        unsigned char* p = &src[(y*w+x)*4];
        p[0] = (unsigned char)(v*230); p[1] = (unsigned char)(v*220); p[2] = (unsigned char)(v*210); p[3] = 255;
    }
    ToonGradient::PARAMS prm = { 0.6f, 0.55f, { 0.86f, 0.98f, 1.25f }, 0.35f };
    ToonGradient::RESULT r = ToonGradient::Enhance(&src[0], &dst[0], w, h, prm);
    CHECK(r.m_iAxis == ((w >= h) ? 0 : 1), "%s axis %d", name, r.m_iAxis);
    CHECK(r.m_bDarkAtStart == darkFirst, "%s dark_at_start %d", name, r.m_bDarkAtStart);
    // brightest texel stays bright (highlight lifts, never darkens), darkest gets bluer and not brighter
    int lit = darkFirst ? axisLen-1 : 0, dark = darkFirst ? 0 : axisLen-1;
    const unsigned char* sl = (w >= h) ? &src[lit*4] : &src[lit*w*4];
    const unsigned char* dl = (w >= h) ? &dst[lit*4] : &dst[lit*w*4];
    const unsigned char* sd = (w >= h) ? &src[dark*4] : &src[dark*w*4];
    const unsigned char* dd = (w >= h) ? &dst[dark*4] : &dst[dark*w*4];
    CHECK(L(dl) >= L(sl) - 0.01f, "%s lit end darkened %.3f -> %.3f", name, L(sl), L(dl));
    CHECK(L(dd) <= L(sd) + 0.01f, "%s dark end brightened %.3f -> %.3f", name, L(sd), L(dd));
    CHECK((dd[2] - dd[0]) > (sd[2] - sd[0]), "%s shadow not cooler (b-r %d -> %d)", name, sd[2]-sd[0], dd[2]-dd[0]);
    CHECK(dd[3] == 255 && dl[3] == 255, "%s alpha changed", name);
    // monotonic luminance along the ramp is preserved
    float prev = -1.f; bool mono = true;
    for (int i = 0; i < axisLen; ++i) {
        int k = darkFirst ? i : axisLen-1-i;
        const unsigned char* p = (w >= h) ? &dst[k*4] : &dst[k*w*4];
        if (L(p) < prev - 0.004f) { mono = false; } prev = L(p);
    }
    CHECK(mono, "%s ramp no longer monotonic", name);
    if (stepped) {  // hard steps stay hard: only 2 distinct colours (no blended in-between bands)
        int distinct = 0; unsigned last = 0xFFFFFFFF;
        for (int i = 0; i < axisLen; ++i) { const unsigned char* p = (w>=h)?&dst[i*4]:&dst[i*w*4]; unsigned c = p[0]|(p[1]<<8)|(p[2]<<16); if (c != last) { ++distinct; last = c; } }
        CHECK(distinct <= 3, "%s stepped ramp got %d bands", name, distinct);
    }
    std::printf("gradient %-22s ok (lit %.2f->%.2f, dark %.2f->%.2f, dark b-r %d->%d)\n", name, L(sl), L(dl), L(sd), L(dd), sd[2]-sd[0], dd[2]-dd[0]);
}

static void TestFlatAndTiny(void)
{
    unsigned char one[4] = { 10, 20, 30, 40 }, out[4];
    ToonGradient::PARAMS prm = { 1, 1, { 0.8f, 1.0f, 1.2f }, 1 };
    ToonGradient::Enhance(one, out, 1, 1, prm);
    CHECK(std::memcmp(one, out, 4) == 0, "1x1 must be unchanged");
    std::vector<unsigned char> flat(8*1*4, 128), o(8*4);
    ToonGradient::Enhance(&flat[0], &o[0], 8, 1, prm);
    CHECK(flat == o, "flat ramp (like pat05) must stay exactly as shipped");
    std::printf("gradient 1x1 / flat unchanged ok\n");
}

static void TestLightRig(void)
{
    // 16x1 ramp like pat01: dark (0.61) on the left, white on the right
    std::vector<unsigned char> src(16 * 4), a(16 * 4), b(16 * 4);
    for (int i = 0; i < 16; ++i) { unsigned char v = (unsigned char)(155 + (100 * i) / 15); src[i*4]=v; src[i*4+1]=v; src[i*4+2]=v; src[i*4+3]=255; }
    ToonGradient::PARAMS off = { 0.6f, 0.55f, { 0.93f, 0.99f, 1.22f }, 0.35f };
    ToonGradient::PARAMS dim = off; dim.m_bLightRig = true; dim.m_fAmbient = 0.5f; dim.m_fKey = 1.0f; dim.m_fWarmth = 0.0f;
    ToonGradient::Enhance(&src[0], &a[0], 16, 1, off);
    ToonGradient::Enhance(&src[0], &b[0], 16, 1, dim);
    CHECK(b[0] + 20 < a[0], "ambient 0.5 must clearly darken the shadow end (%d -> %d)", a[0], b[0]);
    CHECK(std::abs(int(b[15*4]) - int(a[15*4])) <= 2, "ambient must not touch the lit end (%d -> %d)", a[15*4], b[15*4]);
    ToonGradient::PARAMS warm = off; warm.m_bLightRig = true; warm.m_fAmbient = 1.0f; warm.m_fKey = 1.0f; warm.m_fWarmth = 1.0f;
    ToonGradient::Enhance(&src[0], &b[0], 16, 1, warm);
    CHECK(b[15*4+2] + 20 < b[15*4], "warmth must make the lit end warm (r %d b %d)", b[15*4], b[15*4+2]);
    // default Enhanced settings must be clearly visible against the original ramp
    int maxDiff = 0;
    for (int i = 0; i < 16 * 4; ++i) maxDiff = std::max(maxDiff, std::abs(int(a[i]) - int(src[i])));
    CHECK(maxDiff >= 30, "Enhanced defaults change the ramp by only %d/255", maxDiff);
    std::printf("light rig on the ramp ok (default Enhanced ramp changes up to %d/255; ambient, warmth verified)\n", maxDiff);
}

static void TestMesh(void)
{
    // 10 x 6 water rectangle at y = 2 made of 2 big triangles, split across a "sector" seam (4 triangles)
    WaterSurfaceMesh::INPUT in; WaterSurfaceMesh::Begin(in);
    WaterSurfaceMesh::AddTriangle(in, 0,2,0,  5,2,0,  5,2,6);
    WaterSurfaceMesh::AddTriangle(in, 0,2,0,  5,2,6,  0,2,6);
    WaterSurfaceMesh::AddTriangle(in, 5,2,0, 10,2,0, 10,2,6);
    WaterSurfaceMesh::AddTriangle(in, 5,2,0, 10,2,6,  5,2,6);
    WaterSurfaceMesh::OUTPUT out; WaterSurfaceMesh::Finish(in, out, 1.5f, 1.2f, 6.0f, 60000);
    CHECK(out.m_nBoundaryEdge == 6, "boundary edges %d (expected 6: seam edge is interior)", out.m_nBoundaryEdge);
    CHECK(out.m_aIndex.size() % 3 == 0 && !out.m_aIndex.empty(), "index count");
    CHECK(out.m_aVertex.size() > 40, "subdivision too coarse: %zu", out.m_aVertex.size());
    float maxShoreInterior = 0, minShoreEdge = 1, maxDeep = 0; bool yok = true; size_t bad = 0;
    for (size_t i = 0; i < out.m_aVertex.size(); ++i) {
        const WaterSurfaceMesh::VERTEX& v = out.m_aVertex[i];
        if (std::fabs(v.m_y - 2) > 1e-4f) yok = false;
        float d = std::fmin(std::fmin(v.m_x, 10 - v.m_x), std::fmin(v.m_z, 6 - v.m_z));
        if (d < 1e-3f) minShoreEdge = std::fmin(minShoreEdge, v.m_fShore);
        if (d > 1.3f) maxShoreInterior = std::fmax(maxShoreInterior, v.m_fShore);
        maxDeep = std::fmax(maxDeep, v.m_fDeep);
        float expectDeep = (d >= 6) ? 1.f : [&]{ float t = d/6; return t*t*(3-2*t); }();
        if (std::fabs(v.m_fDeep - expectDeep) > 0.02f) ++bad;
    }
    for (size_t i = 0; i < out.m_aIndex.size(); ++i) CHECK(out.m_aIndex[i] < out.m_aVertex.size(), "index out of range");
    CHECK(yok, "vertices left the water plane");
    CHECK(minShoreEdge > 0.99f, "edge vertices shore %.3f", minShoreEdge);
    CHECK(maxShoreInterior < 0.01f, "interior vertices shore %.3f", maxShoreInterior);
    CHECK(maxDeep > 0.49f && maxDeep < 1.01f, "deep max %.3f (centre is 3 units from shore)", maxDeep);
    CHECK(bad == 0, "%zu vertices with wrong depth (distance field)", bad);
    // total area preserved
    double area = 0;
    for (size_t t = 0; t < out.m_aIndex.size(); t += 3) {
        const WaterSurfaceMesh::VERTEX &a = out.m_aVertex[out.m_aIndex[t]], &b = out.m_aVertex[out.m_aIndex[t+1]], &c = out.m_aVertex[out.m_aIndex[t+2]];
        area += 0.5 * std::fabs((b.m_x-a.m_x)*(c.m_z-a.m_z) - (c.m_x-a.m_x)*(b.m_z-a.m_z));
    }
    CHECK(std::fabs(area - 60.0) < 1e-3, "area %.4f != 60", area);
    std::printf("mesh rectangle ok: %zu vertices, %zu tris, %d boundary edges, max segments %d, area %.3f\n",
                out.m_aVertex.size(), out.m_aIndex.size()/3, out.m_nBoundaryEdge, out.m_nSubdivide, area);

    // vertex budget: a 400x400 lake must stay under the cap and still cover the full area
    WaterSurfaceMesh::Begin(in);
    WaterSurfaceMesh::AddTriangle(in, 0,0,0, 400,0,0, 400,0,400);
    WaterSurfaceMesh::AddTriangle(in, 0,0,0, 400,0,400, 0,0,400);
    WaterSurfaceMesh::Finish(in, out, 1.5f, 1.2f, 6.0f, 60000);
    CHECK(out.m_aVertex.size() <= 60000 && !out.m_bCapped, "budget: %zu vertices capped=%d", out.m_aVertex.size(), out.m_bCapped);
    area = 0;
    for (size_t t = 0; t < out.m_aIndex.size(); t += 3) {
        const WaterSurfaceMesh::VERTEX &a = out.m_aVertex[out.m_aIndex[t]], &b = out.m_aVertex[out.m_aIndex[t+1]], &c = out.m_aVertex[out.m_aIndex[t+2]];
        area += 0.5 * std::fabs((b.m_x-a.m_x)*(c.m_z-a.m_z) - (c.m_x-a.m_x)*(b.m_z-a.m_z));
    }
    CHECK(std::fabs(area - 160000.0) < 1.0, "big lake area %.1f", area);
    std::printf("mesh 400x400 lake ok: %zu vertices (cap 60000), segments %d\n", out.m_aVertex.size(), out.m_nSubdivide);

    // degenerate input
    WaterSurfaceMesh::Begin(in);
    WaterSurfaceMesh::AddTriangle(in, 1,1,1, 1,1,1, 2,1,1);
    WaterSurfaceMesh::Finish(in, out);
    CHECK(out.m_aVertex.empty() && out.m_aIndex.empty(), "degenerate triangle produced geometry");
    std::printf("mesh degenerate ok\n");
}

int main()
{
    TestLightRig();
    TestGradient(32, 1, true,  false, "32x1 smooth dark-left");
    TestGradient(32, 1, false, false, "32x1 smooth dark-right");
    TestGradient(1, 64, true,  false, "1x64 vertical");
    TestGradient(8, 2, true,  true,  "8x2 two-step");
    TestGradient(64, 64, true, false, "64x64 2D");
    TestFlatAndTiny();
    TestMesh();
    std::printf(g_fail ? "\n%d FAILURES\n" : "\nALL CPU TESTS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
