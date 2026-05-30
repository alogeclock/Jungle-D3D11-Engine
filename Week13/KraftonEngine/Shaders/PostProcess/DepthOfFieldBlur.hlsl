#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D<float4> SourceLayerTexture : register(t0);

cbuffer DepthOfFieldBlurCB : register(b2)
{
    float2 BlurDirection;
    uint BlurRadius;
    float _Pad;
};

static const int MAX_DOF_BLUR_RADIUS = 12;

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    uint width;
    uint height;
    SourceLayerTexture.GetDimensions(width, height);

    float2 texelSize = 1.0f / float2(max(width, 1), max(height, 1));
    int radius = min((int)BlurRadius, MAX_DOF_BLUR_RADIUS);

    float3 colorSum = 0.0f;
    float colorWeightSum = 0.0f;
    float coverageSum = 0.0f;
    float kernelWeightSum = 0.0f;

    [loop]
    for (int i = -MAX_DOF_BLUR_RADIUS; i <= MAX_DOF_BLUR_RADIUS; ++i)
    {
        if (abs(i) > radius)
        {
            continue;
        }

        float kernelWeight = 1.0f - (abs(i) / (radius + 1.0f));
        float2 sampleUV = input.uv + BlurDirection * texelSize * i;
        float4 sampleValue = SourceLayerTexture.SampleLevel(LinearClampSampler, sampleUV, 0);
        float coverageWeight = max(sampleValue.a, 0.001f);

        colorSum += sampleValue.rgb * kernelWeight * coverageWeight;
        colorWeightSum += kernelWeight * coverageWeight;
        coverageSum += sampleValue.a * kernelWeight;
        kernelWeightSum += kernelWeight;
    }

    float3 blurredColor = colorWeightSum > 0.0001f ? colorSum / colorWeightSum : 0.0f;
    float blurredCoverage = kernelWeightSum > 0.0001f ? coverageSum / kernelWeightSum : 0.0f;
    return float4(blurredColor, saturate(blurredCoverage));
}
