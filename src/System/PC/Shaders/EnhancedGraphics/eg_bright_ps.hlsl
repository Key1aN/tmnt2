// TMNT2 Enhanced Graphics - bloom bright pass + 4x4 box downsample (ps_3_0).
// c0.xy = source texel size, c0.z = threshold, c0.w = soft knee
sampler2D sScene : register(s0);
float4 cParam : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float2 t = cParam.xy;
    float3 c = tex2D(sScene, uv + float2(-t.x, -t.y)).rgb
             + tex2D(sScene, uv + float2( t.x, -t.y)).rgb
             + tex2D(sScene, uv + float2(-t.x,  t.y)).rgb
             + tex2D(sScene, uv + float2( t.x,  t.y)).rgb;
    c *= 0.25;

    float l = max(c.r, max(c.g, c.b));
    float knee = max(cParam.w, 0.0001);
    float soft = clamp(l - cParam.z + knee, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee);
    float contrib = max(soft, l - cParam.z) / max(l, 0.0001);
    return float4(c * saturate(contrib), 1.0);
}
