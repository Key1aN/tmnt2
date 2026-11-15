// TMNT2 Enhanced Graphics - separable 9-tap gaussian using 5 bilinear taps (ps_3_0).
// c0.xy = step (direction * texel size)
sampler2D sSrc : register(s0);
float4 cParam : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float2 d1 = cParam.xy * 1.3846153846;
    float2 d2 = cParam.xy * 3.2307692308;
    float3 c = tex2D(sSrc, uv).rgb * 0.2270270270;
    c += (tex2D(sSrc, uv + d1).rgb + tex2D(sSrc, uv - d1).rgb) * 0.3162162162;
    c += (tex2D(sSrc, uv + d2).rgb + tex2D(sSrc, uv - d2).rgb) * 0.0702702703;
    return float4(c, 1.0);
}
