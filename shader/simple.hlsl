cbuffer WvpCB : register(b0)
{
    float4x4 gWVP; // world * view * proj
};

struct VSIn  { float3 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 svpos : SV_Position; float4 col : COLOR; };

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.svpos = mul(gWVP, float4(i.pos, 1));
    o.col   = i.col;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    return i.col;
}
