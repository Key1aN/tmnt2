// TMNT2 Enhanced Graphics - stylized water overlay pixel shader (ps_3_0).
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

    float4 n1 = tex2D(sWave, uv + cScroll.xy * t);
    float4 n2 = tex2D(sWave, uv * 1.73 + cScroll.zw * t + 0.37);
    float2 d = (n1.xy + n2.xy - 1.0) * cWave.w;
    float4 n3 = tex2D(sWave, uv * 4.1 - cScroll.xy * (t * 1.6) + 0.71);
    d += (n3.xy * 2.0 - 1.0) * (cWave.w * 0.35 * cMisc.y);

    float3 N = normalize(float3(d.x, 1.0, d.y));
    float3 toCam = cCam.xyz - i.wpos;
    float dist = length(toCam);
    float3 V = toCam / max(dist, 0.0001);

    float ndv = saturate(dot(N, V));
    float fres = pow(1.0 - ndv, 4.0);
    float fresT = smoothstep(0.06, 0.45, fres);

    float3 R = reflect(-V, N);
    float3 sky = lerp(cSkyLo.rgb, cSkyHi.rgb, saturate(R.y * 1.6));
    float sp = pow(saturate(dot(R, cLight.xyz)), cLight.w);
    float spT = smoothstep(0.42, 0.52, sp) * 0.85 + sp * 0.15;

    float shore = saturate(i.att.x);
    float deep  = saturate(i.att.y);

    float3 col = lerp(cShallow.rgb, cDeep.rgb, deep);
    col = lerp(col, sky, fresT * cSkyLo.w);
    col += cLightC.rgb * (spT * cSkyHi.w);

    float fn = n1.w * 0.6 + n2.w * 0.4;
    float band = frac(shore * 2.5 - t * 0.25 + fn * 0.35);
    float foamLines = smoothstep(0.80, 0.86, band) * shore * shore;
    float foamEdge = smoothstep(0.80, 0.96, shore + (fn - 0.5) * 0.25);
    float foam = saturate(max(foamLines * 0.8, foamEdge)) * cLightC.w;
    col = lerp(col, float3(0.94, 0.97, 1.0), foam);

    float a = lerp(cShallow.w, cDeep.w, deep) + fresT * 0.25 * cSkyLo.w + spT * cSkyHi.w * 0.5 + foam;

    float f = saturate((cWave.y - dist) / max(cWave.y - cWave.x, 0.001));
    col = lerp(cFog.rgb, col, lerp(1.0, f, cFog.w));

    return float4(saturate(col), saturate(a) * cMisc.x);
}
