#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"

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

float ComputeSignedCoC(float linearDepth)
{
    float f = max(FocalLength, 0.001f);
    float n = max(FNumber, 0.1f);
    float sensorWidth = max(SensorWidth, 0.001f);

    float zf = max(FocusDistance * DepthToMillimeters, f + 0.001f);
    float z = max(linearDepth * DepthToMillimeters, f + 0.001f);

    float apertureDiameter = f / n;
    float vf = (f * zf) / max(zf - f, 0.001f);
    float v = (f * z) / max(z - f, 0.001f);
    float signedCoCMm = apertureDiameter * ((vf - v) / max(v, 0.001f));

    uint width;
    uint height;
    SceneDepthTexture.GetDimensions(width, height);

    float signedCoCPixels = signedCoCMm * (float)max(width, 1) / sensorWidth;
    if (signedCoCPixels < 0.0f)
    {
        signedCoCPixels *= saturate(NearCoCScale);
    }

    float focusRange = max(FocusRange, 0.0f);
    if (focusRange > 0.0001f)
    {
        float focusDelta = abs(linearDepth - FocusDistance);
        signedCoCPixels *= smoothstep(focusRange * 0.5f, focusRange, focusDelta);
    }

    float nearLimit = max(min(MaxNearCoCRadius, MaxCoCRadius), 0.0f);
    float farLimit = max(min(MaxFarCoCRadius, MaxCoCRadius), 0.0f);
    return clamp(signedCoCPixels, -nearLimit, farLimit);
}

float PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);
    float rawDepth = SceneDepthTexture.Load(int3(coord, 0));
    float linearDepth = LinearizeSceneDepth(rawDepth);
    return ComputeSignedCoC(linearDepth);
}
