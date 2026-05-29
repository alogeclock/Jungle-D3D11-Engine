/**
 * @file SceneDepthTypes.h
 * @brief SceneDepth 사용에 필요한 공용 타입 정의.
 *
 * 포함 타입:
 * - ESceneDepthSpace: Depth 값 해석 방식
 * - FSceneDepthDesc: SceneDepth 사용 설정
 * - FCameraDepthViewData: CameraDepth가 Rendering으로 전달하는 View 데이터
 */

#pragma once

#include "CameraFocusTypes.h"
#include "CameraDOFSettings.h"
#include "Core/CoreTypes.h"
#include "Math/MathUtils.h"

/** SceneDepth 값 해석 방식 */
enum class ESceneDepthSpace : uint8
{
    SDS_DeviceZ,    // Projection 이후 Device Z
    SDS_LinearDepth // Camera 기준 선형 거리
};

/** SceneDepth 사용 설정 */
struct FSceneDepthDesc
{
    bool bUseSceneDepth = true; // SceneDepth 사용 여부

    float NearPlane = 0.1f;    // Camera Near Plane
    float FarPlane = 10000.0f; // Camera Far Plane

    bool bLinearizeDepth = true; // Depth 선형화 여부

    ESceneDepthSpace DepthSpace = ESceneDepthSpace::SDS_LinearDepth; // Depth 값 해석 방식
};

/** CameraDepth가 Rendering / PostProcess로 전달하는 공용 데이터 */
struct FCameraDepthViewData
{
    FCameraFocusSettings FocusSettings; // Camera Focus 설정
    FCameraDOFSettings   DOFSettings;   // DOF 효과 설정
    FSceneDepthDesc      SceneDepthDesc; // SceneDepth 해석 설정
};

/** DOF Blur Amount 계산식 */
inline float ComputeDOFBlurAmount(float PixelDepth, float FocusDistance, float FocusRange)
{
    if (FocusRange <= EPSILON)
    {
        return 0.0f;
    }

    return FMath::Clamp(abs(PixelDepth - FocusDistance) / FocusRange, 0.0f, 1.0f);
}
