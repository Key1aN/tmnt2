// TMNT2 Enhanced Graphics - final composite: optional FXAA-lite, bloom, colour grade, vignette (ps_3_0).
// c0 = (1/W, 1/H, bloom intensity, fxaa 0/1)
// c1 = (contrast, saturation, vignette, 0)
// c2 = shadow split-tone offset rgb, amount
// c3 = highlight split-tone offset rgb, amount
sampler2D sScene : register(s0);
sampler2D sBloom : register(s1);
float4 cA : register(c0);
float4 cB : register(c1);
float4 cS : register(c2);
float4 cH : register(c3);

static const float3 LUMA = float3(0.299, 0.587, 0.114);

// step(a, b) without comparison instructions (1 when b >= a).
float StepF(float a, float b) { return saturate((b - a) * 65536.0 + 1.0); }

float3 Fxaa(float2 uv)
{
    float2 t = cA.xy;
    float3 rgbM = tex2D(sScene, uv).rgb;
    float lM = dot(rgbM, LUMA);
    float lN = dot(tex2D(sScene, uv + float2(0.0, -t.y)).rgb, LUMA);
    float lS = dot(tex2D(sScene, uv + float2(0.0,  t.y)).rgb, LUMA);
    float lW = dot(tex2D(sScene, uv + float2(-t.x, 0.0)).rgb, LUMA);
    float lE = dot(tex2D(sScene, uv + float2( t.x, 0.0)).rgb, LUMA);

    float lMin = min(lM, min(min(lN, lS), min(lW, lE)));
    float lMax = max(lM, max(max(lN, lS), max(lW, lE)));
    float range = lMax - lMin;
    float edge = StepF(max(0.0312, lMax * 0.125), range);

    // Horizontal edge (luma changes vertically) -> blend vertically, and vice versa.
    float gradV = abs(lN + lS - 2.0 * lM);
    float gradH = abs(lW + lE - 2.0 * lM);
    float horizontal = StepF(gradH, gradV);
    float2 stepDir = lerp(float2(t.x, 0.0), float2(0.0, t.y), horizontal);
    float lPos = lerp(lE, lS, horizontal);
    float lNeg = lerp(lW, lN, horizontal);
    float side = StepF(abs(lNeg - lM), abs(lPos - lM)) * 2.0 - 1.0;

    float sub = saturate(abs((lN + lS + lW + lE) * 0.25 - lM) / max(range, 0.0001));
    float blend = sub * sub * 0.75;
    float3 rgbB = tex2D(sScene, uv + stepDir * side * 0.5).rgb;
    return lerp(rgbM, lerp(rgbM, rgbB, 0.5 + blend * 0.5), edge);
}

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    // Branch-free: the FXAA result is blended in with weight 0 or 1 (c0.w).
    float3 c = lerp(tex2D(sScene, uv).rgb, Fxaa(uv), cA.w);

    float3 bloom = tex2D(sBloom, uv).rgb;
    c += bloom * cA.z * (1.0 - saturate(c) * 0.5);

    float l = dot(c, LUMA);
    c = lerp(float3(l, l, l), c, cB.y);
    c = (c - 0.5) * cB.x + 0.5;

    float lc = saturate(l);
    c += cS.rgb * (cS.w * (1.0 - lc) * (1.0 - lc));
    c += cH.rgb * (cH.w * lc * lc);

    float2 d = (uv - 0.5) * 1.41421356;
    c *= 1.0 - cB.z * smoothstep(0.45, 1.0, dot(d, d));

    return float4(saturate(c), 1.0);
}
