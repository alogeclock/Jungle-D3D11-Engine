#ifndef FUNCTIONS_HLSL
#define FUNCTIONS_HLSL

#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"

// Model -> View -> Projection 변환
float4 ApplyMVP(float3 pos)
{
    float4 world = mul(Model, float4(pos, 1.0f));
    float4 view = mul(View, world);
    return mul(Projection, view);
}

// View -> Projection 변환 (CPU 빌보드용 — 이미 월드 좌표인 정점)
float4 ApplyVP(float3 worldPos)
{
    return mul(Projection, mul(View, float4(worldPos, 1.0f)));
}

// 와이어프레임 모드 적용 — baseColor를 WireframeRGB로 대체
float3 ApplyWireframe(float3 baseColor)
{
    return lerp(baseColor, WireframeRGB, bIsWireframe);
}

// 폰트 아틀라스 배경 디스카드 판정
bool ShouldDiscardFontPixel(float sampledRed)
{
    return sampledRed < 0.1f;
}

// Fullscreen Triangle VS — SV_VertexID 3개로 화면 전체를 덮는 오버사이즈 삼각형 생성
PS_Input_UV FullscreenTriangleVS(uint vertexID)
{
    PS_Input_UV output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

#endif // FUNCTIONS_HLSL
