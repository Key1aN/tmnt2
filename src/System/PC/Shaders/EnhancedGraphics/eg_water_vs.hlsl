// TMNT2 Enhanced Graphics - stylized water overlay vertex shader (vs_3_0).
// c0..c3 = columns of (View * Projection), c4.x = vertical lift in world units.
float4 cVP0 : register(c0);
float4 cVP1 : register(c1);
float4 cVP2 : register(c2);
float4 cVP3 : register(c3);
float4 cParam : register(c4);

struct VSIn  { float3 pos : POSITION; float4 att : TEXCOORD0; };
struct VSOut { float4 pos : POSITION; float3 wpos : TEXCOORD0; float4 att : TEXCOORD1; };

VSOut main(VSIn i)
{
    VSOut o;
    float4 p = float4(i.pos.x, i.pos.y + cParam.x, i.pos.z, 1.0);
    o.pos  = float4(dot(p, cVP0), dot(p, cVP1), dot(p, cVP2), dot(p, cVP3));
    o.wpos = p.xyz;
    o.att  = i.att;
    return o;
}
