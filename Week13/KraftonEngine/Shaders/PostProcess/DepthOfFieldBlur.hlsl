#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D<float4> SourceLayerTexture : register(t0);
Texture2D<float> FullCoCTexture : register(t1);

cbuffer DepthOfFieldBlurCB : register(b2)
{
    uint BlurRadius;
    float LayerSign;
    float MaxLayerRadius;
    float _Pad;
};

static const int MAX_DOF_BLUR_RADIUS = 12;
static const int DOF_DISK_SAMPLE_COUNT = 25;
static const float2 DOF_DISK_SAMPLES[DOF_DISK_SAMPLE_COUNT] =
{
    float2( 0.000f,  0.000f),
    float2( 0.533f,  0.000f),
    float2(-0.533f,  0.000f),
    float2( 0.000f,  0.533f),
    float2( 0.000f, -0.533f),
    float2( 0.377f,  0.377f),
    float2(-0.377f,  0.377f),
    float2( 0.377f, -0.377f),
    float2(-0.377f, -0.377f),
    float2( 0.866f,  0.500f),
    float2(-0.866f,  0.500f),
    float2( 0.866f, -0.500f),
    float2(-0.866f, -0.500f),
    float2( 0.500f,  0.866f),
    float2(-0.500f,  0.866f),
    float2( 0.500f, -0.866f),
    float2(-0.500f, -0.866f),
    float2( 1.000f,  0.000f),
    float2(-1.000f,  0.000f),
    float2( 0.000f,  1.000f),
    float2( 0.000f, -1.000f),
    float2( 0.259f,  0.966f),
    float2(-0.259f,  0.966f),
    float2( 0.259f, -0.966f),
    float2(-0.259f, -0.966f)
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    uint cocWidth;
    uint cocHeight;
    FullCoCTexture.GetDimensions(cocWidth, cocHeight);

    uint cappedBlurRadius = min(BlurRadius, (uint)MAX_DOF_BLUR_RADIUS);
    float radius = min(max(MaxLayerRadius, 0.0f), (float)cappedBlurRadius);
    if (radius <= 0.0001f)
    {
        return SourceLayerTexture.SampleLevel(LinearClampSampler, input.uv, 0);
    }

    float2 cocTexelSize = 1.0f / float2(max(cocWidth, 1), max(cocHeight, 1));

    float3 colorSum = 0.0f;
    float colorWeightSum = 0.0f;
    float coverageSum = 0.0f;
    float coverageWeightSum = 0.0f;

    [unroll]
    for (int i = 0; i < DOF_DISK_SAMPLE_COUNT; ++i)
    {
        float2 diskOffset = DOF_DISK_SAMPLES[i];
        float distancePixels = length(diskOffset) * radius;
        float2 sampleUV = input.uv + diskOffset * radius * cocTexelSize;

        float sampleCoC = FullCoCTexture.SampleLevel(LinearClampSampler, sampleUV, 0) * LayerSign;
        float sampleRadius = clamp(sampleCoC, 0.0f, radius);
        float diskCoverage = smoothstep(distancePixels - 1.0f, distancePixels + 1.0f, sampleRadius);
        float4 sampleValue = SourceLayerTexture.SampleLevel(LinearClampSampler, sampleUV, 0);
        float layerCoverage = saturate(sampleValue.a) * diskCoverage;

        colorSum += sampleValue.rgb * layerCoverage;
        colorWeightSum += layerCoverage;
        coverageSum += layerCoverage;
        coverageWeightSum += diskCoverage;
    }

    float3 blurredColor = colorWeightSum > 0.0001f ? colorSum / colorWeightSum : 0.0f;
    float blurredCoverage = coverageWeightSum > 0.0001f ? coverageSum / coverageWeightSum : 0.0f;
    return float4(blurredColor, saturate(blurredCoverage));
}
