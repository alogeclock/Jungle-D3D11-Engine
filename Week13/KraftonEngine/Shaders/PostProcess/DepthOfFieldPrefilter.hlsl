#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D<float> FullCoCTexture : register(t0);

cbuffer DepthOfFieldCB : register(b2)
{
    float FocalLength;
    float FNumber;
    float FocusDistance;
    float FocusRange;
    float MaxCoCRadius;
    float MaxNearCoCRadius;
    float MaxFarCoCRadius;
    float SensorWidth;
    float DepthToMillimeters;
    uint DebugView;
    uint BlurRadius;
    float NearCoCScale;
};

struct FPrefilterOutput
{
    float4 NearLayer : SV_Target0;
    float4 FarLayer : SV_Target1;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float ComputeLayerAmount(float signedRadius, float maxRadius)
{
    float amount = saturate(signedRadius / max(maxRadius, 0.0001f));
    return amount * smoothstep(0.02f, 0.2f, amount);
}

void AccumulateLayer(float2 uv, inout float3 nearColor, inout float nearWeight, inout float nearCoverage,
    inout float3 farColor, inout float farWeight, inout float farCoverage)
{
    float4 sceneColor = SceneColorTexture.SampleLevel(LinearClampSampler, uv, 0);
    float signedCoC = FullCoCTexture.SampleLevel(PointClampSampler, uv, 0);

    float nearAmount = ComputeLayerAmount(-signedCoC, MaxNearCoCRadius);
    float farAmount = ComputeLayerAmount(signedCoC, MaxFarCoCRadius);

    nearColor += sceneColor.rgb * nearAmount;
    nearWeight += nearAmount;
    nearCoverage = max(nearCoverage, nearAmount);

    farColor += sceneColor.rgb * farAmount;
    farWeight += farAmount;
    farCoverage = max(farCoverage, farAmount);
}

FPrefilterOutput PS(PS_Input_UV input)
{
    uint width;
    uint height;
    SceneColorTexture.GetDimensions(width, height);

    float2 texelSize = 1.0f / float2(max(width, 1), max(height, 1));
    float2 offsets[4] =
    {
        float2(-0.5f, -0.5f),
        float2( 0.5f, -0.5f),
        float2(-0.5f,  0.5f),
        float2( 0.5f,  0.5f)
    };

    float3 nearColor = 0.0f;
    float nearWeight = 0.0f;
    float nearCoverage = 0.0f;
    float3 farColor = 0.0f;
    float farWeight = 0.0f;
    float farCoverage = 0.0f;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        AccumulateLayer(input.uv + offsets[i] * texelSize, nearColor, nearWeight, nearCoverage,
            farColor, farWeight, farCoverage);
    }

    float farForegroundMask = 1.0f - saturate(nearCoverage);

    FPrefilterOutput output;
    output.NearLayer = float4(nearWeight > 0.0001f ? nearColor / nearWeight : 0.0f, nearCoverage);
    output.FarLayer = float4(farWeight > 0.0001f ? farColor / farWeight : 0.0f, farCoverage * farForegroundMask);
    return output;
}
