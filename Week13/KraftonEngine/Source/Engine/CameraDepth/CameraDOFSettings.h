/**
 * @file CameraDOFSettings.h
 * @brief Depth of Field 효과에 필요한 Camera 설정 정의.
 *
 * 포함 타입:
 * - FCameraDOFSettings: DOF PostProcess에 전달되는 초점 / 블러 설정
 */

#pragma once

#include "CameraDepthTypes.h"

/** Depth of Field 설정 */
struct FCameraDOFSettings
{
    bool bEnableDOF = false; // DOF 사용 여부

    float FocalLength = 10.0f; // 초점 길이(렌즈에서 센서까지의 거리)
    float FocusRange = 5.0f;  // 초점으로 인정할 범위

    float Aperture = 4.0f;      // 조리개 값
    float MaxBlurRadius = 8.0f; // 최대 Blur 반경

    ECameraDepthDebugView DebugView = ECameraDepthDebugView::CDDV_None; // DOF 디버그 표시 모드
};
