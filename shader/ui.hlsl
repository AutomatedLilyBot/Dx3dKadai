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
    float emissive;      // 发光强度（>1.0发光）
    float uvOffsetX;     // UV偏移（用于数字显示）
    float uvOffsetY;
    float padding;
};

SamplerState gSampler : register(s0);
Texture2D gTexture : register(t0);

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.svpos = float4(i.pos, 0.0f, 1.0f);
    // 应用UV偏移（用于数字显示的UV滚动）
    o.uv = i.uv + float2(uvOffsetX, uvOffsetY);
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    float4 texColor = gTexture.Sample(gSampler, i.uv);
    float4 finalColor = texColor * tint;

    // 应用发光效果（emissive > 1.0 时增强亮度）
    finalColor.rgb *= emissive;

    return finalColor;
}
