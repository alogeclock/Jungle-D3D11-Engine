#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/ForwardLighting.hlsli"

Texture2D DiffuseTexture : register(t0);

static const float g_DecalShininess = 32.0f;

// b2 (PerShader0)
cbuffer DecalBuffer : register(b2)
{
    float4x4 DecalWorldToLocal;
    float4 DecalColor;
}

PS_Input_Decal VS(VS_Input_PNCT input)
{
    PS_Input_Decal output;

    float4 worldPos = mul(Model, float4(input.position, 1.0f));
    output.position = mul(Projection, mul(View, worldPos));
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul((float3x3) NormalMatrix, input.normal));
    return output;
}

float4 PS(PS_Input_Decal input) : SV_TARGET
{
    float3 decalLocalPos = mul(DecalWorldToLocal, float4(input.worldPos, 1.0f)).xyz;

    if (abs(decalLocalPos.x) > 0.5f || abs(decalLocalPos.y) > 0.5f || abs(decalLocalPos.z) > 0.5f)
    {
        discard;
    }

    float2 uv;
    uv.x = decalLocalPos.y + 0.5f;
    uv.y = 0.5f - decalLocalPos.z;

    float4 texColor = DiffuseTexture.Sample(LinearWrapSampler, uv);
    if (texColor.a < 0.001f)
    {
        discard;
    }

    float4 baseColor = texColor * DecalColor;

    // Forward Lighting
    float3 N = normalize(input.normal);
    float3 V = normalize(CameraWorldPos - input.worldPos);

    float3 diffuse  = AccumulateDiffuse(input.worldPos, N, input.position);
    float3 specular = AccumulateSpecular(input.worldPos, N, V, g_DecalShininess, input.position);

    float3 finalColor = baseColor.rgb * diffuse + specular;
    return float4(ApplyWireframe(finalColor), baseColor.a);
}
