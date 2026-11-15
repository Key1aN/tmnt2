// TMNT2 Enhanced Graphics - full-screen triangle-strip vertex shader (vs_3_0).
// Positions arrive already in clip space with the D3D9 half-pixel offset applied on the CPU.
struct VSIn  { float4 pos : POSITION; float2 uv : TEXCOORD0; };
struct VSOut { float4 pos : POSITION; float2 uv : TEXCOORD0; };

VSOut main(VSIn i)
{
    VSOut o;
    o.pos = i.pos;
    o.uv  = i.uv;
    return o;
}
