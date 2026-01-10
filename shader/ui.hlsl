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
    float uvOffsetX;     // UV偏移（用于数字显示的sprite偏移）
    float uvOffsetY;
    float uvScaleX;      // UV缩放（用于切割sprite）
    float uvScaleY;
    float padding1;
    float padding2;
};

SamplerState gSampler : register(s0);
Texture2D gTexture : register(t0);

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.svpos = float4(i.pos, 0.0f, 1.0f);
    // 应用UV缩放和偏移来实现sprite切割
    // 1. 缩放UV到sprite大小 (默认1.0表示整张图)
    // 2. 偏移到对应sprite位置
    o.uv = i.uv * float2(uvScaleX, uvScaleY) + float2(uvOffsetX, uvOffsetY);
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
