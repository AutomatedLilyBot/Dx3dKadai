cbuffer ViewCB : register(b0)
{
    row_major float4x4 gViewProj;
}

cbuffer LightCB : register(b1)
{
    float3 gLightDir;
    float  gPadding1;
    float3 gLightColor;
    float  gPadding2;
    float3 gAmbientColor;
    float  gPadding3;
}

cbuffer MaterialCB : register(b2)
{
    float4 gBaseColorFactor;
}

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSIn
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float4 col    : COLOR;
    float2 uv     : TEXCOORD;
    float4 row0   : TEXCOORD1;
    float4 row1   : TEXCOORD2;
    float4 row2   : TEXCOORD3;
    float4 row3   : TEXCOORD4;
    float  alpha  : TEXCOORD5;
};

struct VSOut
{
    float4 svpos     : SV_Position;
    float3 worldNorm : NORMAL;
    float4 col       : COLOR;
    float2 uv        : TEXCOORD;
    float  alpha     : TEXCOORD1;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    float4x4 world = float4x4(i.row0, i.row1, i.row2, i.row3);
    float4 worldPos = mul(float4(i.pos, 1), world);
    o.svpos = mul(worldPos, gViewProj);
    o.worldNorm = mul(float4(i.normal, 0), world).xyz;
    o.col = i.col;
    o.uv = i.uv;
    o.alpha = i.alpha;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    float4 texColor = gTexture.Sample(gSampler, i.uv);
    float3 N = normalize(i.worldNorm);
    float3 L = -gLightDir;
    float diffuseFactor = max(dot(N, L), 0.0f);
    float3 diffuse = gLightColor * diffuseFactor;
    float3 lighting = gAmbientColor + diffuse;
    float4 finalColor = float4(lighting, 1.0f) * texColor * i.col * gBaseColorFactor;
    finalColor.a *= i.alpha;
    return finalColor;
}
