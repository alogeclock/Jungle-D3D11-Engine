/**
 * @file CameraDOFSettings.h
 * @brief Depth of Field 효과에 필요한 Camera 설정 정의.
 *
 * 포함 타입:
 * - FCameraDOFSettings: DOF PostProcess에 전달되는 초점 / 블러 설정
 */

#pragma once

#include "CameraDepthTypes.h"
#include "CameraDOFSettings.generated.h"

/** Depth of Field 설정 */
USTRUCT()
struct FCameraDOFSettings
{
    GENERATED_BODY(FCameraDOFSettings)

    UPROPERTY(Edit, Category="Depth Of Field", DisplayName="Enable DOF")
    bool bEnableDOF = false; // DOF 사용 여부

    UPROPERTY(Edit, Category="Depth Of Field", DisplayName="Focal Length", Min=0.1f, Max=300.0f, Speed=0.1f)
    float FocalLength = 10.0f; // 초점 길이(렌즈에서 센서까지의 거리)

    UPROPERTY(Edit, Category="Depth Of Field", DisplayName="Focus Range", Min=0.0f, Max=100000.0f, Speed=1.0f)
    float FocusRange = 5.0f;  // 초점으로 인정할 범위

    UPROPERTY(Edit, Category="Depth Of Field", DisplayName="Aperture F-Stop", Min=0.1f, Max=64.0f, Speed=0.1f)
    float Aperture = 4.0f;      // 조리개 값

    UPROPERTY(Edit, Category="Depth Of Field", DisplayName="Max Blur Radius", Min=0.0f, Max=12.0f, Speed=0.5f)
    float MaxBlurRadius = 8.0f; // 최대 Blur 반경

    UPROPERTY(Edit, Category="Depth Of Field", DisplayName="Near CoC Scale", Min=0.0f, Max=1.0f, Speed=0.05f)
    float NearCoCScale = 0.5f; // Near blur 완화 스케일. 1.0이면 물리식 그대로 사용

    ECameraDepthDebugView DebugView = ECameraDepthDebugView::CDDV_None; // DOF 디버그 표시 모드
};
