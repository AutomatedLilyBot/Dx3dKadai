// ---- 常量缓冲 ----
cbuffer FrameCB : register(b0)
{
    float4x4 gView;
    float4x4 gProj;

    float3   gCameraPosWS;  float _pad0;
    float3   gAmbient;      float _pad1;

    float3   gDirLightDirectionWS; float gDirLightIntensity;
    float3   gDirLightColor;       float _pad2;

    uint     gPointLightCount; float3 _pad3;
};

cbuffer ObjectCB : register(b1)
{
    float4x4 gWorld;
    float4x4 gWorldNormal;
};

cbuffer MaterialCB : register(b2)
{
    float3 gAlbedoFactor;  float gUseAlbedoTexture;
    float3 gSpecularColor; float gSpecularPower;
    float3 gEmissive;      float _padM;
};

struct PointLight { float3 positionWS; float intensity; float3 color; float range; };
cbuffer PointLightsCB : register(b3)
{
    PointLight gPointLights[4];
}

// ---- 资源 ----
Texture2D    gAlbedoTex : register(t0);
SamplerState gSampler    : register(s0);

// ---- I/O ----
struct VSIn  { float3 pos:POSITION; float3 norm:NORMAL; float2 uv:TEXCOORD0; };
struct VSOut { float4 svpos:SV_Position; float3 posWS:TEXCOORD0; float3 normalWS:TEXCOORD1; float2 uv:TEXCOORD2; };

// ---- 光照工具 ----
float3 ApplyPointLight(PointLight L, float3 P, float3 N, float3 V, float3 specColor, float specPower)
{
    float3 Lvec = L.positionWS - P;
    float  dist = length(Lvec);
    float3 Ldir = (dist > 1e-5) ? (Lvec / dist) : 0;
    float  NdotL= saturate(dot(N, Ldir));

    float  atten= 1.0 / (1.0 + (dist / max(L.range, 1e-3)) * (dist / max(L.range, 1e-3)));
    float3 diff = L.color * (L.intensity * NdotL * atten);

    float3 H    = normalize(Ldir + V);
    float  NdotH= saturate(dot(N, H));
    float3 spec = specColor * (L.intensity * pow(NdotH, specPower) * atten);
    return diff + spec;
}

float3 ApplyDirLight(float3 dirWS, float intensity, float3 color, float3 P, float3 N, float3 V, float3 specColor, float specPower)
{
    float3 Ldir = normalize(-dirWS);
    float  NdotL= saturate(dot(N, Ldir));
    float3 diff = color * (intensity * NdotL);

    float3 H    = normalize(Ldir + V);
    float  NdotH= saturate(dot(N, H));
    float3 spec = specColor * (intensity * pow(NdotH, specPower));
    return diff + spec;
}

// ---- VS/PS ----
VSOut VSMain(VSIn i)
{
    VSOut o;
    float4 posWS = mul(gWorld, float4(i.pos, 1));
    float3 nWS   = normalize( mul((float3x3)gWorldNormal, i.norm) );
    float4 posVS = mul(gView, posWS);
    float4 posCS = mul(gProj, posVS);
    o.svpos      = posCS;
    o.posWS      = posWS.xyz;
    o.normalWS   = nWS;
    o.uv         = i.uv;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    float3 V = normalize(gCameraPosWS - i.posWS);
    float3 N = normalize(i.normalWS);

    float3 albedo = gAlbedoFactor;
    if (gUseAlbedoTexture > 0.5) {
        albedo *= gAlbedoTex.Sample(gSampler, i.uv).rgb;
    }

    float3 color = gAmbient * albedo;
    color += albedo * ApplyDirLight(gDirLightDirectionWS, gDirLightIntensity, gDirLightColor, i.posWS, N, V, gSpecularColor, gSpecularPower);

    [unroll]
    for (uint k = 0; k < gPointLightCount; ++k) {
        color += albedo * ApplyPointLight(gPointLights[k], i.posWS, N, V, gSpecularColor, gSpecularPower);
    }

    color += gEmissive;
    return float4(color, 1);
}
