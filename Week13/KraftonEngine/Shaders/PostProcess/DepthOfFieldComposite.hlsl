#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D<float4> FarLayerTexture : register(t0);
Texture2D<float4> NearLayerTexture : register(t1);
Texture2D<float> FullCoCTexture : register(t2);

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

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float LinearizeSceneDepth(float rawDepth)
{
    float viewZ = Projection[3][2] / (rawDepth - Projection[2][2]);
    return abs(viewZ);
}

float4 VisualizeSceneDepth(PS_Input_UV input)
{
    int2 coord = int2(input.position.xy);
    float rawDepth = SceneDepthTexture.Load(int3(coord, 0));
    float linearDepth = LinearizeSceneDepth(rawDepth);
    float focusDepth = max(FocusDistance, 0.0001f);
    float depth01 = saturate(linearDepth / (focusDepth * 2.0f));
    return float4(depth01, depth01, depth01, 1.0f);
}

float4 VisualizeFocusPlane(PS_Input_UV input)
{
    int2 coord = int2(input.position.xy);
    float rawDepth = SceneDepthTexture.Load(int3(coord, 0));
    float linearDepth = LinearizeSceneDepth(rawDepth);
    float focusFalloff = saturate(abs(linearDepth - FocusDistance) / max(FocusRange, 0.0001f));
    float focusMask = 1.0f - focusFalloff;
    return float4(1.0f - focusMask, focusMask, 0.0f, 1.0f);
}

float4 VisualizeBlurAmount(PS_Input_UV input)
{
    float signedCoC = FullCoCTexture.SampleLevel(PointClampSampler, input.uv, 0);
    float blurAmount = saturate(abs(signedCoC) / max(MaxCoCRadius, 0.0001f));
    return float4(blurAmount, blurAmount, blurAmount, 1.0f);
}

float ComputeLayerMask(float signedRadius, float maxRadius)
{
    float amount = saturate(signedRadius / max(maxRadius, 0.0001f));
    return amount * smoothstep(0.02f, 0.2f, amount);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    if (DebugView == 1)
    {
        return VisualizeSceneDepth(input);
    }
    if (DebugView == 2)
    {
        return VisualizeFocusPlane(input);
    }
    if (DebugView == 3)
    {
        return VisualizeBlurAmount(input);
    }

    float4 sceneColor = SceneColorTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    float4 farLayer = FarLayerTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    float4 nearLayer = NearLayerTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    float signedCoC = FullCoCTexture.SampleLevel(PointClampSampler, input.uv, 0);

    float farMask = ComputeLayerMask(signedCoC, MaxFarCoCRadius);
    float farBlend = saturate(farLayer.a) * farMask;
    float nearBlend = saturate(nearLayer.a);

    float3 color = lerp(sceneColor.rgb, farLayer.rgb, farBlend);
    color = lerp(color, nearLayer.rgb, nearBlend);
    return float4(color, sceneColor.a);
}
