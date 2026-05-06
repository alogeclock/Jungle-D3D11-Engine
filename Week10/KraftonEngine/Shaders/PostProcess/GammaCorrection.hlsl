#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

float3 LinearToSRGB(float3 Linear)
{
    Linear = saturate(Linear);
    return lerp(
        Linear * 12.92f,
        1.055f * pow(Linear, 1.0f / 2.4f) - 0.055f,
        step(0.0031308f, Linear)
    );
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 SceneColor = SceneColorTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    return float4(LinearToSRGB(SceneColor.rgb), SceneColor.a);
}
