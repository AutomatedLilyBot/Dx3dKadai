// Skybox shader for cube map rendering
// This shader renders a skybox using a cube map texture

cbuffer SkyboxCB : register(b0)
{
    row_major float4x4 gViewProj;  // View-Projection matrix (view has translation removed)
}

// Cube map texture and sampler
TextureCube gCubeMap : register(t0);
SamplerState gSampler : register(s0);

struct VSIn
{
    float3 pos : POSITION;  // Local position (unit cube vertices)
};

struct VSOut
{
    float4 svpos : SV_Position;  // Clip space position
    float3 texCoord : TEXCOORD;  // 3D texture coordinate for cube map sampling
};

VSOut VSMain(VSIn i)
{
    VSOut o;

    // Transform position to clip space
    // Note: The view matrix should have translation removed before being passed in
    o.svpos = mul(float4(i.pos, 1.0f), gViewProj);

    // Force depth to be at far plane (z = w)
    // This ensures skybox is rendered behind all other geometry
    o.svpos.z = o.svpos.w;

    // Use local position as texture coordinate for cube map
    // The cube map will be sampled in the direction from center to this point
    o.texCoord = i.pos;

    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    // Sample cube map using 3D texture coordinate
    float4 color = gCubeMap.Sample(gSampler, i.texCoord);

    return color;
}
