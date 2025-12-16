struct VSIn
{
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct VSOut
{
    float4 svpos : SV_Position;
    float2 uv    : TEXCOORD0;
};

cbuffer UiCB : register(b0)
{
    float4 tint;
};

SamplerState gSampler : register(s0);
Texture2D gTexture : register(t0);

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.svpos = float4(i.pos, 0.0f, 1.0f);
    o.uv = i.uv;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    float4 texColor = gTexture.Sample(gSampler, i.uv);
    return texColor * tint;
}
