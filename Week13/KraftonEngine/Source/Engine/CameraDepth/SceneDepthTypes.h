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
struct FCameraViewSettings
{
    FCameraFocusSettings FocusSettings; // Camera Focus 설정
    FCameraDOFSettings   DOFSettings;   // DOF 효과 설정
    FSceneDepthDesc      SceneDepthDesc; // SceneDepth 해석 설정
};

struct FCameraDepthViewData
{
	// SceneDepth 사용 여부
	bool bUseSceneDepth = true;
	bool bLinearizeDepth = true;

	bool bEnableDOF = false; // DOF 사용 여부
	ESceneDepthSpace DepthSpace = ESceneDepthSpace::SDS_LinearDepth; // Depth 값 해석 방식

	float NearPlane = 0.1f;
	float FarPlane = 10000.0f;

	float FocalLength = 10.0f; // 초점 길이(렌즈에서 센서까지의 거리)
	float FocusRange = 5.0f;  // 초점으로 인정할 범위

	float Aperture = 4.0f;      // 조리개 값
	// CoC 최대 반지름, 픽셀 단위
	float MaxCoCRadius = 8.0f;

	// Near / Far blur 분리용
	float MaxNearCoCRadius = 8.0f;
	float MaxFarCoCRadius = 8.0f;

	float CurrentFocusDistance = 10.0f; // 현재 보간된 FocusDistance

	ECameraDepthDebugView DebugView = ECameraDepthDebugView::CDDV_None; // DOF 디버그 표시 모드
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
