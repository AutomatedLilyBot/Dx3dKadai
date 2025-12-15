cbuffer WvpCB : register(b0)
{
    row_major float4x4 gWorld;     // World matrix
    row_major float4x4 gWVP;       // World-View-Projection matrix
}

cbuffer LightCB : register(b1)
{
    float3 gLightDir;              // Directional light direction (normalized)
    float  gPadding1;
    float3 gLightColor;            // Light color
    float  gPadding2;
    float3 gAmbientColor;          // Ambient light color
    float  gPadding3;
}

cbuffer MaterialCB : register(b2)
{
    float4 gBaseColorFactor;       // Material base color factor (RGBA)
}

// Texture and sampler
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSIn
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float4 col    : COLOR;
    float2 uv     : TEXCOORD;
};

struct VSOut
{
    float4 svpos     : SV_Position;
    float3 worldNorm : NORMAL;
    float4 col       : COLOR;
    float2 uv        : TEXCOORD;
};

VSOut VSMain(VSIn i)
{
    VSOut o;

    // Transform position to clip space
    o.svpos = mul(float4(i.pos, 1), gWVP);

    // Transform normal to world space (assume uniform scaling, otherwise use inverse-transpose)
    o.worldNorm = mul(float4(i.normal, 0), gWorld).xyz;

    // Pass through color and UV
    o.col = i.col;
    o.uv = i.uv;

    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    // DEBUG: Visualize UV coordinates as colors
    // return float4(i.uv.x, i.uv.y, 0, 1);

    // Sample texture first for debugging
    float4 texColor = gTexture.Sample(gSampler, i.uv);

    // DEBUG: Return texture color only to test
    // return texColor;

    // Normalize the interpolated normal
    float3 N = normalize(i.worldNorm);

    // Light direction (pointing towards light)
    float3 L = -gLightDir;

    // Calculate diffuse lighting (Lambert)
    float diffuseFactor = max(dot(N, L), 0.0f);
    float3 diffuse = gLightColor * diffuseFactor;

    // Combine ambient and diffuse
    float3 lighting = gAmbientColor + diffuse;

    // Final color = lighting * texture * vertex color * material base color
    float4 finalColor = float4(lighting, 1.0f) * texColor * i.col * gBaseColorFactor;

    return finalColor;
}
