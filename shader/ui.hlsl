cbuffer OrthoCB : register(b0)
{
    float2 gOffset;    // unused placeholder for alignment
    float2 gScale;     // unused placeholder for alignment
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSIn
{
    float3 pos : POSITION;
    float3 norm : NORMAL; // unused, kept for buffer layout compatibility
    float2 uv  : TEXCOORD;
};

struct VSOut
{
    float4 svpos : SV_Position;
    float2 uv    : TEXCOORD;
};

VSOut VSMain(VSIn input)
{
    VSOut o;
    o.svpos = float4(input.pos, 1.0f);
    o.uv = input.uv;
    return o;
}

float4 PSMain(VSOut input) : SV_Target
{
    return gTexture.Sample(gSampler, input.uv);
}
