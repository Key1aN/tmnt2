// Renders every Enhanced Graphics shader on a real D3D9 device and compares the result,
// pixel by pixel, with a C++ reference implementation of the same math.
// Built with MinGW; runs on Windows or under Wine (wined3d). Exit code 0 = all passed.
#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

#include "../../../src/System/PC/PCEnhancedGraphicsShaders.inl"

static IDirect3DDevice9* g_dev = nullptr;
static int g_fail = 0;

struct V4 { float x, y, z, w; };
static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float smooth(float e0, float e1, float x) { float t = clamp01((x - e0) / (e1 - e0)); return t * t * (3 - 2 * t); }
static float fracf(float x) { return x - std::floor(x); }

/* ---------- textures ---------- */
struct IMG { int w, h; std::vector<float> rgba; };
static IMG MakeImg(int w, int h, unsigned seed, int kind)
{
    IMG im{ w, h, std::vector<float>(w * h * 4) };
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        seed = seed * 1664525u + 1013904223u;
        float n = ((seed >> 8) & 255) / 255.0f;
        float* p = &im.rgba[(y * w + x) * 4];
        if (kind == 0) {          // "game frame": flat cel areas, a hard black ink edge, bright spots, noise
            bool ink = (x % 23 == 0) || (y % 19 == 0);
            float base = ((x / 11 + y / 7) % 3) * 0.3f + 0.15f;
            p[0] = ink ? 0.02f : clamp01(base + 0.05f * n);
            p[1] = ink ? 0.02f : clamp01(base * 0.9f + 0.03f);
            p[2] = ink ? 0.02f : clamp01(base * 0.7f + ((x * y) % 5) * 0.05f);
            if (((x * 7 + y * 3) % 41) == 0) { p[0] = p[1] = p[2] = 1.0f; }   // highlights for bloom
        } else {                  // random
            p[0] = n; seed = seed * 1664525u + 1013904223u; p[1] = ((seed >> 8) & 255) / 255.0f;
            seed = seed * 1664525u + 1013904223u; p[2] = ((seed >> 8) & 255) / 255.0f;
        }
        seed = seed * 1664525u + 1013904223u;
        p[3] = ((seed >> 8) & 255) / 255.0f;
        for (int c = 0; c < 4; ++c) p[c] = std::floor(p[c] * 255.0f + 0.5f) / 255.0f;   // what 8-bit storage holds
    }
    return im;
}
static IDirect3DTexture9* Upload(const IMG& im)
{
    IDirect3DTexture9* t = nullptr;
    g_dev->CreateTexture(im.w, im.h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, nullptr);
    D3DLOCKED_RECT lr; t->LockRect(0, &lr, nullptr, 0);
    for (int y = 0; y < im.h; ++y) for (int x = 0; x < im.w; ++x) {
        const float* p = &im.rgba[(y * im.w + x) * 4];
        unsigned r = unsigned(p[0] * 255 + .5f), g = unsigned(p[1] * 255 + .5f), b = unsigned(p[2] * 255 + .5f), a = unsigned(p[3] * 255 + .5f);
        ((unsigned*)((char*)lr.pBits + y * lr.Pitch))[x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    t->UnlockRect(0);
    return t;
}
static V4 TexBilinearClamp(const IMG& im, float u, float v)
{
    float fx = u * im.w - 0.5f, fy = v * im.h - 0.5f;
    int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy); float ax = fx - x0, ay = fy - y0;
    auto at = [&](int x, int y, int c) { x = std::min(std::max(x, 0), im.w - 1); y = std::min(std::max(y, 0), im.h - 1); return im.rgba[(y * im.w + x) * 4 + c]; };
    float o[4];
    for (int c = 0; c < 4; ++c) o[c] = lerpf(lerpf(at(x0, y0, c), at(x0 + 1, y0, c), ax), lerpf(at(x0, y0 + 1, c), at(x0 + 1, y0 + 1, c), ax), ay);
    return { o[0], o[1], o[2], o[3] };
}
static V4 TexPointWrap(const IMG& im, float u, float v)
{
    int x = (int)std::floor(fracf(u) * im.w) % im.w, y = (int)std::floor(fracf(v) * im.h) % im.h;
    const float* p = &im.rgba[(y * im.w + x) * 4];
    return { p[0], p[1], p[2], p[3] };
}

/* ---------- render helpers ---------- */
struct FSV { float x, y, z, w, u, v; };
static IDirect3DVertexDeclaration9* g_fsDecl = nullptr;
static IDirect3DVertexShader9* g_fsVS = nullptr;

static IDirect3DTexture9* MakeRT(int w, int h) { IDirect3DTexture9* t = nullptr; g_dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t, nullptr); return t; }

static void DrawFS(IDirect3DTexture9* rt, int w, int h)
{
    IDirect3DSurface9* s; rt->GetSurfaceLevel(0, &s); g_dev->SetRenderTarget(0, s); s->Release();
    D3DVIEWPORT9 vp = { 0, 0, (DWORD)w, (DWORD)h, 0, 1 }; g_dev->SetViewport(&vp);
    float hx = 1.0f / w, hy = 1.0f / h;
    FSV q[4] = { { -1 - hx, 1 + hy, 0, 1, 0, 0 }, { 1 - hx, 1 + hy, 0, 1, 1, 0 }, { -1 - hx, -1 + hy, 0, 1, 0, 1 }, { 1 - hx, -1 + hy, 0, 1, 1, 1 } };
    g_dev->SetVertexDeclaration(g_fsDecl); g_dev->SetVertexShader(g_fsVS);
    g_dev->BeginScene(); g_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(FSV)); g_dev->EndScene();
}
static std::vector<float> Read(IDirect3DTexture9* rt, int w, int h)
{
    IDirect3DSurface9 *s, *m; rt->GetSurfaceLevel(0, &s);
    g_dev->CreateOffscreenPlainSurface(w, h, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &m, nullptr);
    g_dev->GetRenderTargetData(s, m);
    D3DLOCKED_RECT lr; m->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    std::vector<float> out(w * h * 4);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        unsigned c = ((unsigned*)((char*)lr.pBits + y * lr.Pitch))[x];
        out[(y * w + x) * 4 + 0] = ((c >> 16) & 255) / 255.0f; out[(y * w + x) * 4 + 1] = ((c >> 8) & 255) / 255.0f;
        out[(y * w + x) * 4 + 2] = (c & 255) / 255.0f;         out[(y * w + x) * 4 + 3] = (c >> 24) / 255.0f;
    }
    m->UnlockRect(); m->Release(); s->Release();
    return out;
}
static void SetSamp(int st, DWORD addr, DWORD filt)
{
    g_dev->SetSamplerState(st, D3DSAMP_ADDRESSU, addr); g_dev->SetSamplerState(st, D3DSAMP_ADDRESSV, addr);
    g_dev->SetSamplerState(st, D3DSAMP_MINFILTER, filt); g_dev->SetSamplerState(st, D3DSAMP_MAGFILTER, filt);
    g_dev->SetSamplerState(st, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
}

static void Compare(const char* name, const std::vector<float>& got, const std::vector<V4>& want, int channels, float tol, float allowBadFrac)
{
    size_t n = want.size(), bad = 0; float worst = 0; size_t worstAt = 0;
    for (size_t i = 0; i < n; ++i) {
        const float* g = &got[i * 4]; const float w4[4] = { want[i].x, want[i].y, want[i].z, want[i].w };
        float e = 0; for (int c = 0; c < channels; ++c) e = std::max(e, std::fabs(g[c] - clamp01(w4[c])));
        if (e > tol) ++bad;
        if (e > worst) { worst = e; worstAt = i; }
    }
    bool ok = bad <= (size_t)(allowBadFrac * n);
    if (!ok) ++g_fail;
    std::printf("%-34s %s  pixels=%zu  over_tol=%zu (%.2f%%)  max_err=%.4f (%.1f/255) at %zu\n", name, ok ? "PASS" : "FAIL", n, bad, 100.0 * bad / n, worst, worst * 255, worstAt);
    if (!ok) {
        const float* g = &got[worstAt * 4];
        std::printf("    got  %.4f %.4f %.4f %.4f\n    want %.4f %.4f %.4f %.4f\n", g[0], g[1], g[2], g[3], want[worstAt].x, want[worstAt].y, want[worstAt].z, want[worstAt].w);
    }
}

/* ---------- references (same math as the HLSL, written independently) ---------- */
static const float LW[3] = { 0.299f, 0.587f, 0.114f };
static float luma(V4 c) { return c.x * LW[0] + c.y * LW[1] + c.z * LW[2]; }
static float stepf(float a, float b) { return clamp01((b - a) * 65536.0f + 1.0f); }

static V4 RefBright(const IMG& s, float u, float v, const float* c0)
{
    float t[2] = { c0[0], c0[1] };
    V4 a = TexBilinearClamp(s, u - t[0], v - t[1]), b = TexBilinearClamp(s, u + t[0], v - t[1]), c = TexBilinearClamp(s, u - t[0], v + t[1]), d = TexBilinearClamp(s, u + t[0], v + t[1]);
    V4 m = { (a.x + b.x + c.x + d.x) * .25f, (a.y + b.y + c.y + d.y) * .25f, (a.z + b.z + c.z + d.z) * .25f, 1 };
    float l = std::max(m.x, std::max(m.y, m.z)), knee = std::max(c0[3], 0.0001f);
    float soft = std::min(std::max(l - c0[2] + knee, 0.0f), 2 * knee); soft = soft * soft / (4 * knee);
    float k = clamp01(std::max(soft, l - c0[2]) / std::max(l, 0.0001f));
    return { m.x * k, m.y * k, m.z * k, 1 };
}
static V4 RefBlur(const IMG& s, float u, float v, const float* c0)
{
    float d1x = c0[0] * 1.3846153846f, d1y = c0[1] * 1.3846153846f, d2x = c0[0] * 3.2307692308f, d2y = c0[1] * 3.2307692308f;
    V4 a = TexBilinearClamp(s, u, v), b = TexBilinearClamp(s, u + d1x, v + d1y), c = TexBilinearClamp(s, u - d1x, v - d1y), d = TexBilinearClamp(s, u + d2x, v + d2y), e = TexBilinearClamp(s, u - d2x, v - d2y);
    auto mix = [&](float A, float B, float C, float D, float E) { return A * 0.2270270270f + (B + C) * 0.3162162162f + (D + E) * 0.0702702703f; };
    return { mix(a.x, b.x, c.x, d.x, e.x), mix(a.y, b.y, c.y, d.y, e.y), mix(a.z, b.z, c.z, d.z, e.z), 1 };
}
static V4 RefComposite(const IMG& sc, const IMG& bl, float u, float v, const float* cA, const float* cB, const float* cS, const float* cH)
{
    auto S = [&](float du, float dv) { return TexBilinearClamp(sc, u + du, v + dv); };
    V4 M = S(0, 0);
    float lM = luma(M), lN = luma(S(0, -cA[1])), lS = luma(S(0, cA[1])), lW = luma(S(-cA[0], 0)), lE = luma(S(cA[0], 0));
    float lMin = std::min(lM, std::min(std::min(lN, lS), std::min(lW, lE))), lMax = std::max(lM, std::max(std::max(lN, lS), std::max(lW, lE)));
    float range = lMax - lMin, edge = stepf(std::max(0.0312f, lMax * 0.125f), range);
    float gradV = std::fabs(lN + lS - 2 * lM), gradH = std::fabs(lW + lE - 2 * lM), hor = stepf(gradH, gradV);
    float sdx = lerpf(cA[0], 0, hor), sdy = lerpf(0, cA[1], hor);
    float lPos = lerpf(lE, lS, hor), lNeg = lerpf(lW, lN, hor);
    float side = stepf(std::fabs(lNeg - lM), std::fabs(lPos - lM)) * 2 - 1;
    float sub = clamp01(std::fabs((lN + lS + lW + lE) * .25f - lM) / std::max(range, 0.0001f)), blend = sub * sub * .75f;
    V4 B = S(sdx * side * .5f, sdy * side * .5f);
    float f[3] = { M.x, M.y, M.z }, b3[3] = { B.x, B.y, B.z }, fx[3];
    for (int i = 0; i < 3; ++i) fx[i] = lerpf(f[i], lerpf(f[i], b3[i], 0.5f + blend * 0.5f), edge);
    float c[3]; for (int i = 0; i < 3; ++i) c[i] = lerpf(f[i], fx[i], cA[3]);
    V4 bb = TexBilinearClamp(bl, u, v); float b4[3] = { bb.x, bb.y, bb.z };
    for (int i = 0; i < 3; ++i) c[i] += b4[i] * cA[2] * (1 - clamp01(c[i]) * .5f);
    float l = c[0] * LW[0] + c[1] * LW[1] + c[2] * LW[2];
    for (int i = 0; i < 3; ++i) { c[i] = lerpf(l, c[i], cB[1]); c[i] = (c[i] - .5f) * cB[0] + .5f; }
    float lc = clamp01(l);
    for (int i = 0; i < 3; ++i) c[i] += cS[i] * (cS[3] * (1 - lc) * (1 - lc)) + cH[i] * (cH[3] * lc * lc);
    float dx = (u - .5f) * 1.41421356f, dy = (v - .5f) * 1.41421356f;
    float vig = 1 - cB[2] * smooth(0.45f, 1.0f, dx * dx + dy * dy);
    return { clamp01(c[0] * vig), clamp01(c[1] * vig), clamp01(c[2] * vig), 1 };
}

int main()
{
    HWND hwnd = CreateWindowA("STATIC", "eg", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, nullptr, nullptr);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { std::printf("no d3d9\n"); return 2; }
    D3DADAPTER_IDENTIFIER9 id; d3d->GetAdapterIdentifier(0, 0, &id);
    D3DCAPS9 caps; d3d->GetDeviceCaps(0, D3DDEVTYPE_HAL, &caps);
    std::printf("adapter: %s  vs=%lu.%lu ps=%lu.%lu\n", id.Description, (caps.VertexShaderVersion >> 8) & 255, caps.VertexShaderVersion & 255, (caps.PixelShaderVersion >> 8) & 255, caps.PixelShaderVersion & 255);
    D3DPRESENT_PARAMETERS pp = {}; pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN; pp.hDeviceWindow = hwnd;
    if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &g_dev))) { std::printf("CreateDevice failed\n"); return 2; }

    struct { const char* n; const DWORD* code; bool vs; } sh[] = {
        { "eg_fullscreen_vs", (const DWORD*)g_EgFullscreenVS, true }, { "eg_bright_ps", (const DWORD*)g_EgBrightPS, false },
        { "eg_blur_ps", (const DWORD*)g_EgBlurPS, false }, { "eg_composite_ps", (const DWORD*)g_EgCompositePS, false },
        { "eg_water_vs", (const DWORD*)g_EgWaterVS, true }, { "eg_water_ps", (const DWORD*)g_EgWaterPS, false } };
    void* obj[6] = {};
    for (int i = 0; i < 6; ++i) {
        HRESULT hr = sh[i].vs ? g_dev->CreateVertexShader(sh[i].code, (IDirect3DVertexShader9**)&obj[i]) : g_dev->CreatePixelShader(sh[i].code, (IDirect3DPixelShader9**)&obj[i]);
        std::printf("create %-20s %s (hr=0x%08lX)\n", sh[i].n, SUCCEEDED(hr) ? "OK" : "FAILED", hr);
        if (FAILED(hr)) ++g_fail;
    }
    if (g_fail) return 1;
    g_fsVS = (IDirect3DVertexShader9*)obj[0];
    D3DVERTEXELEMENT9 fsd[] = { { 0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0 }, { 0, 16, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0 }, D3DDECL_END() };
    g_dev->CreateVertexDeclaration(fsd, &g_fsDecl);
    g_dev->SetRenderState(D3DRS_ZENABLE, FALSE); g_dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    const int W = 128, H = 96, QW = W / 4, QH = H / 4;
    IMG scene = MakeImg(W, H, 1234, 0);
    IDirect3DTexture9* tScene = Upload(scene);

    /* bright pass + downsample */
    {
        float c0[4] = { 1.0f / W, 1.0f / H, 0.80f, 0.25f };
        IDirect3DTexture9* rt = MakeRT(QW, QH);
        g_dev->SetPixelShader((IDirect3DPixelShader9*)obj[1]); g_dev->SetPixelShaderConstantF(0, c0, 1);
        g_dev->SetTexture(0, tScene); SetSamp(0, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR);
        DrawFS(rt, QW, QH);
        std::vector<float> got = Read(rt, QW, QH);
        std::vector<V4> want(QW * QH);
        for (int y = 0; y < QH; ++y) for (int x = 0; x < QW; ++x) want[y * QW + x] = RefBright(scene, (x + .5f) / QW, (y + .5f) / QH, c0);
        Compare("bright pass (quarter res)", got, want, 3, 3.5f / 255, 0.0);
        rt->Release();
    }
    /* blur, both directions */
    for (int dir = 0; dir < 2; ++dir) {
        IMG src = MakeImg(QW, QH, 99 + dir, 1);
        IDirect3DTexture9* t = Upload(src); IDirect3DTexture9* rt = MakeRT(QW, QH);
        float c0[4] = { dir == 0 ? 1.0f / QW : 0, dir == 1 ? 1.0f / QH : 0, 0, 0 };
        g_dev->SetPixelShader((IDirect3DPixelShader9*)obj[2]); g_dev->SetPixelShaderConstantF(0, c0, 1);
        g_dev->SetTexture(0, t); SetSamp(0, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR);
        DrawFS(rt, QW, QH);
        std::vector<float> got = Read(rt, QW, QH); std::vector<V4> want(QW * QH);
        for (int y = 0; y < QH; ++y) for (int x = 0; x < QW; ++x) want[y * QW + x] = RefBlur(src, (x + .5f) / QW, (y + .5f) / QH, c0);
        Compare(dir == 0 ? "blur horizontal" : "blur vertical", got, want, 3, 3.5f / 255, 0.0);
        t->Release(); rt->Release();
    }
    /* composite: pass-through, grade only, full Enhanced, High (FXAA) */
    {
        IMG bloom = MakeImg(QW, QH, 555, 1);
        IDirect3DTexture9* tBloom = Upload(bloom);
        struct CASE { const char* n; float a[4], b[4], s[4], h[4]; } cases[] = {
            { "composite pass-through (OFF math)", { 1.f / W, 1.f / H, 0, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
            { "composite Enhanced (bloom+grade)",  { 1.f / W, 1.f / H, 0.14f, 0 }, { 1.04f, 1.06f, 0.12f, 0 }, { -0.02f, -0.004f, 0.03f, 0.36f }, { 0.02f, 0.008f, -0.016f, 0.27f } },
            { "composite High (FXAA on)",          { 1.f / W, 1.f / H, 0.17f, 1 }, { 1.04f, 1.06f, 0.12f, 0 }, { -0.02f, -0.004f, 0.03f, 0.36f }, { 0.02f, 0.008f, -0.016f, 0.27f } },
        };
        for (auto& c : cases) {
            IDirect3DTexture9* rt = MakeRT(W, H);
            g_dev->SetPixelShader((IDirect3DPixelShader9*)obj[3]);
            g_dev->SetPixelShaderConstantF(0, c.a, 1); g_dev->SetPixelShaderConstantF(1, c.b, 1); g_dev->SetPixelShaderConstantF(2, c.s, 1); g_dev->SetPixelShaderConstantF(3, c.h, 1);
            g_dev->SetTexture(0, tScene); g_dev->SetTexture(1, tBloom); SetSamp(0, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR); SetSamp(1, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR);
            DrawFS(rt, W, H);
            std::vector<float> got = Read(rt, W, H); std::vector<V4> want(W * H);
            for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) want[y * W + x] = RefComposite(scene, bloom, (x + .5f) / W, (y + .5f) / H, c.a, c.b, c.s, c.h);
            Compare(c.n, got, want, 3, 3.5f / 255, 0.002);
            if (c.a[2] == 0 && c.a[3] == 0) {   // pass-through must reproduce the input exactly
                size_t diff = 0; for (int i = 0; i < W * H; ++i) for (int k = 0; k < 3; ++k) if (std::fabs(got[i * 4 + k] - scene.rgba[i * 4 + k]) > 0.5f / 255) ++diff;
                std::printf("    pass-through identical texels: %s (%zu channel differences)\n", diff ? "NO" : "yes", diff);
                if (diff) ++g_fail;
            }
            rt->Release();
        }
        tBloom->Release();
    }
    /* water: orthographic top-down view of an 8x8 patch at y=0, constant shore/deep per case */
    {
        const int WW = 96, WH = 96;
        IMG wave = MakeImg(64, 64, 4242, 1);
        IDirect3DTexture9* tWave = Upload(wave);
        D3DVERTEXELEMENT9 wd[] = { { 0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 }, { 0, 12, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_TEXCOORD, 0 }, D3DDECL_END() };
        IDirect3DVertexDeclaration9* wdecl; g_dev->CreateVertexDeclaration(wd, &wdecl);
        // clip.x = x/4 - 1 ; clip.y = 1 - z/4 ; clip.z = 0.5 ; w = 1  (columns of VP as the module uploads them)
        float vs[5][4] = { { 0.25f, 0, 0, -1 }, { 0, 0, -0.25f, 1 }, { 0, 0, 0, 0.5f }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } };
        struct WC { const char* n; float shore, deep, fogOn, t; } wcs[] = {
            { "water shore (foam, shallow)", 0.92f, 0.05f, 0, 1.7f }, { "water open (deep, fresnel/spec)", 0.0f, 1.0f, 0, 12.3f }, { "water with fog", 0.3f, 0.6f, 1, 5.0f } };
        for (auto& c : wcs) {
            float ps[11][4] = {
                { 4, 6, -3, c.t }, { 0.30f, 0.80f, -0.52f, 64 }, { 0.30f, 0.55f, 0.58f, 0.16f }, { 0.08f, 0.20f, 0.30f, 0.42f },
                { 0.55f, 0.65f, 0.80f, 0.55f }, { 0.80f, 0.88f, 1.0f, 0.80f }, { 1.0f, 0.97f, 0.94f, 0.70f }, { 0.40f, 0.42f, 0.50f, c.fogOn },
                { 3.0f, 9.0f, 0.18f, 0.35f }, { 0.020f, 0.013f, -0.016f, 0.021f }, { 1.0f, 1.0f, 0, 0 } };
            float s = c.shore, d = c.deep;
            float verts[4][7] = { { 0, 0, 0, s, d, 0, 0 }, { 8, 0, 0, s, d, 0, 0 }, { 0, 0, 8, s, d, 0, 0 }, { 8, 0, 8, s, d, 0, 0 } };
            IDirect3DTexture9* rt = MakeRT(WW, WH);
            IDirect3DSurface9* su; rt->GetSurfaceLevel(0, &su); g_dev->SetRenderTarget(0, su); su->Release();
            D3DVIEWPORT9 vp = { 0, 0, WW, WH, 0, 1 }; g_dev->SetViewport(&vp);
            g_dev->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0);
            g_dev->SetVertexDeclaration(wdecl); g_dev->SetVertexShader((IDirect3DVertexShader9*)obj[4]); g_dev->SetPixelShader((IDirect3DPixelShader9*)obj[5]);
            g_dev->SetVertexShaderConstantF(0, &vs[0][0], 5); g_dev->SetPixelShaderConstantF(0, &ps[0][0], 11);
            g_dev->SetTexture(0, tWave); SetSamp(0, D3DTADDRESS_WRAP, D3DTEXF_POINT);
            g_dev->BeginScene(); g_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, 28); g_dev->EndScene();
            std::vector<float> got = Read(rt, WW, WH); std::vector<V4> want(WW * WH);
            for (int py = 0; py < WH; ++py) for (int px = 0; px < WW; ++px) {
                // D3D9: pixel centres sit on integer screen coordinates and this VS applies no half-pixel shift
                float wx = px * 8.0f / WW, wz = py * 8.0f / WH, wy = 0;
                float t = ps[0][3], u = wx * ps[8][2], v = wz * ps[8][2];
                V4 n1 = TexPointWrap(wave, u + ps[9][0] * t, v + ps[9][1] * t);
                V4 n2 = TexPointWrap(wave, u * 1.73f + ps[9][2] * t + .37f, v * 1.73f + ps[9][3] * t + .37f);
                V4 n3 = TexPointWrap(wave, u * 4.1f - ps[9][0] * t * 1.6f + .71f, v * 4.1f - ps[9][1] * t * 1.6f + .71f);
                float dx = (n1.x + n2.x - 1) * ps[8][3] + (n3.x * 2 - 1) * ps[8][3] * .35f * ps[10][1];
                float dz = (n1.y + n2.y - 1) * ps[8][3] + (n3.y * 2 - 1) * ps[8][3] * .35f * ps[10][1];
                float nl = std::sqrt(dx * dx + 1 + dz * dz), N[3] = { dx / nl, 1 / nl, dz / nl };
                float tc[3] = { ps[0][0] - wx, ps[0][1] - wy, ps[0][2] - wz }, dist = std::sqrt(tc[0] * tc[0] + tc[1] * tc[1] + tc[2] * tc[2]);
                float V[3] = { tc[0] / dist, tc[1] / dist, tc[2] / dist };
                float ndv = clamp01(N[0] * V[0] + N[1] * V[1] + N[2] * V[2]), fres = std::pow(1 - ndv, 4.0f), fresT = smooth(0.06f, 0.45f, fres);
                float vn = V[0] * N[0] + V[1] * N[1] + V[2] * N[2], R[3] = { 2 * vn * N[0] - V[0], 2 * vn * N[1] - V[1], 2 * vn * N[2] - V[2] };
                float sky[3]; for (int k = 0; k < 3; ++k) sky[k] = lerpf(ps[4][k], ps[5][k], clamp01(R[1] * 1.6f));
                float sp = std::pow(clamp01(R[0] * ps[1][0] + R[1] * ps[1][1] + R[2] * ps[1][2]), ps[1][3]);
                float spT = smooth(0.42f, 0.52f, sp) * .85f + sp * .15f;
                float col[3]; for (int k = 0; k < 3; ++k) { col[k] = lerpf(ps[2][k], ps[3][k], d); col[k] = lerpf(col[k], sky[k], fresT * ps[4][3]); col[k] += ps[6][k] * spT * ps[5][3]; }
                float fn = n1.w * .6f + n2.w * .4f, band = fracf(s * 2.5f - t * .25f + fn * .35f);
                float foamLines = smooth(.80f, .86f, band) * s * s, foamEdge = smooth(.80f, .96f, s + (fn - .5f) * .25f);
                float foam = clamp01(std::max(foamLines * .8f, foamEdge)) * ps[6][3];
                const float fc[3] = { .94f, .97f, 1.0f }; for (int k = 0; k < 3; ++k) col[k] = lerpf(col[k], fc[k], foam);
                float a = lerpf(ps[2][3], ps[3][3], d) + fresT * .25f * ps[4][3] + spT * ps[5][3] * .5f + foam;
                float f = clamp01((ps[8][1] - dist) / std::max(ps[8][1] - ps[8][0], 0.001f));
                for (int k = 0; k < 3; ++k) col[k] = lerpf(ps[7][k], col[k], lerpf(1, f, ps[7][3]));
                want[py * WW + px] = { clamp01(col[0]), clamp01(col[1]), clamp01(col[2]), clamp01(a) * ps[10][0] };
            }
            // point sampling can land on the neighbouring texel when a coordinate sits on a texel edge: allow a few
            Compare(c.n, got, want, 4, 4.5f / 255, 0.03);
            rt->Release();
        }
        wdecl->Release(); tWave->Release();
    }

    std::printf(g_fail ? "\nSHADER TESTS: %d FAILED\n" : "\nSHADER TESTS: ALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
