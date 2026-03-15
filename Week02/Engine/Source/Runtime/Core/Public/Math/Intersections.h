#pragma once

#include "Plane.h"
#include "Matrix.h"

// Triangle 판정 : 뫼러-트럼보어 교차 알고리즘
static bool RayIntersectsTriangle(
    const FVector<float> &RayOrigin, 
    const FVector<float> &RayDirection, 
    const FVector<float> &V0, 
    const FVector<float> &V1,
    const FVector<float> &V2,
    float &OutT)                // Out: 교차점까지의 거리
{
    const float EPSILON = 1e-6f;

    FVector<float> Edge1 = V1 - V0;
    FVector<float> Edge2 = V2 - V0;

    // 행렬식 계산 (Ray와 Edge2의 외적)
    FVector<float> H = FVector<float>::CrossProduct(RayDirection, Edge2);
    float          Det = FVector<float>::DotProduct(Edge1, H);

    // Det ≈ 0 이면 Ray가 Triangle과 평행
    if (fabs(Det) < EPSILON)
        return false;

    float InvDet = 1.0f / Det;

    // 무게중심 좌표 U 계산
    FVector<float> S = RayOrigin - V0;
    float          U = InvDet * FVector<float>::DotProduct(S, H);
    if (U < 0.0f || U > 1.0f)
        return false;

    // 무게중심 좌표 V 계산
    FVector<float> Q = FVector<float>::CrossProduct(S, Edge1);
    float          V = InvDet * FVector<float>::DotProduct(RayDirection, Q);
    if (V < 0.0f || U + V > 1.0f)
        return false;

    // T 계산 (교차점까지 거리)
    float T = InvDet * FVector<float>::DotProduct(Edge2, Q);
    if (T < EPSILON)
        return false; // Ray 뒤쪽 교차 무시

    OutT = T;
    return true;
}