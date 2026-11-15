// TMNT2 Enhanced Graphics - stylized water overlay pixel shader (ps_3_0).
// v1.1.2: drawn with modulate-2x blending (dest * 2 * out). 0.5 = leave the game's own water
// pixel unchanged, so the overlay enhances the stage's water texture and lighting instead of
// painting over it: dark corners stay dark, murky water stays murky.
// att.x = shore proximity (1 at the edge of the water mesh), att.y = depth proxy (0 shallow .. 1 deep)
sampler2D sWave : register(s0);   // rgb = tileable wave normal, a = foam noise
float4 cCam     : register(c0);   // camera position, time (s)
float4 cLight   : register(c1);   // direction towards the light, specular power
float4 cShallow : register(c2);   // shallow colour, shallow alpha
float4 cDeep    : register(c3);   // deep colour, deep alpha
float4 cSkyLo   : register(c4);   // horizon colour, reflection amount
float4 cSkyHi   : register(c5);   // zenith colour, specular intensity
float4 cLightC  : register(c6);   // light colour, foam amount
float4 cFog     : register(c7);   // fog colour, fog enable
float4 cWave    : register(c8);   // fog start, fog end, wave uv scale, wave strength
float4 cScroll  : register(c9);   // layer 1 scroll xy, layer 2 scroll xy (uv per second)
float4 cMisc    : register(c10);  // overall strength, detail layer 0/1

struct PSIn { float3 wpos : TEXCOORD0; float4 att : TEXCOORD1; };

float4 main(PSIn i) : COLOR0
{
    float t = cCam.w;
    float2 uv = i.wpos.xz * cWave.z;

    float3 toCam = cCam.xyz - i.wpos;
    float dist = length(toCam);
    float3 V = toCam / max(dist, 0.0001);

    float4 n1 = tex2D(sWave, uv + cScroll.xy * t);
    float4 n2 = tex2D(sWave, uv * 1.73 + cScroll.zw * t + 0.37);
    float2 d = (n1.xy + n2.xy - 1.0) * cWave.w;
    float4 n3 = tex2D(sWave, uv * 4.1 - cScroll.xy * (t * 1.6) + 0.71);
    d += (n3.xy * 2.0 - 1.0) * (cWave.w * 0.35 * cMisc.y);
    d *= 1.0 / (1.0 + dist * 0.025);        // calmer far away: no tiling shimmer

    float3 N = normalize(float3(d.x, 1.0, d.y));
    float ndv = saturate(dot(N, V));
    float fres = pow(1.0 - ndv, 4.0);
    float fresT = smoothstep(0.06, 0.45, fres);

    float3 R = reflect(-V, N);
    float3 sky = lerp(cSkyLo.rgb, cSkyHi.rgb, saturate(R.y * 1.6));
    float sp = pow(saturate(dot(R, cLight.xyz)), cLight.w);
    float spT = smoothstep(0.42, 0.52, sp) * 0.85 + sp * 0.15;

    float shore = saturate(i.att.x);
    float deep  = saturate(i.att.y);
    float3 LW = float3(0.299, 0.587, 0.114);

    // gentle hue bias toward the stage's own water colour (hue only, not brightness)
    float3 tint = lerp(cShallow.rgb, cDeep.rgb, deep);
    tint = tint / max(dot(tint, LW), 0.05);
    float3 m = lerp(float3(1.0, 1.0, 1.0), tint, 0.12 + 0.10 * deep);

    // wave shading relative to a flat surface
    m *= 1.0 + (dot(N, cLight.xyz) - cLight.y) * 1.5;

    // sky reflection at grazing angles, as a relative brightening
    float3 skyT = sky / max(dot(sky, LW), 0.05);
    m *= lerp(float3(1.0, 1.0, 1.0), skyT * 1.25, fresT * cSkyLo.w * 0.6);

    // toon glints
    m += cLightC.rgb * (spT * cSkyHi.w * 0.9);

    // shoreline foam (brightens what is there)
    float fn = n1.w * 0.6 + n2.w * 0.4;
    float band = frac(shore * 2.5 - t * 0.25 + fn * 0.35);
    float foamLines = smoothstep(0.80, 0.86, band) * shore * shore;
    float foamEdge = smoothstep(0.80, 0.96, shore + (fn - 0.5) * 0.25);
    float foam = saturate(max(foamLines * 0.8, foamEdge)) * cLightC.w;
    m = lerp(m, float3(1.8, 1.8, 1.8), foam * 0.8);

    // amount: weaker in the shallows, full in open water
    float amt = saturate(lerp(cShallow.w, cDeep.w, deep) * 2.4) * cMisc.x;
    m = lerp(float3(1.0, 1.0, 1.0), m, amt);

    // distance fog: fade toward a slightly darker, fog-tinted neutral
    float f = saturate((cWave.y - dist) / max(cWave.y - cWave.x, 0.001));
    float3 fogN = lerp(float3(1.0, 1.0, 1.0), float3(0.85, 0.85, 0.85) + cFog.rgb * 0.3, cFog.w);
    m = lerp(fogN, m, lerp(1.0, f, cFog.w));

    return float4(saturate(m * 0.5), 1.0);
}
