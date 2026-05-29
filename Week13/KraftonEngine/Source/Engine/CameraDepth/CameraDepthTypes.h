/**
 * @file CameraDepthTypes.h
 * @brief CameraDepth 공용 enum / forward declaration 정의.
 *
 * 포함 타입:
 * - ECameraDepthDebugView: CameraDepth 디버그 표시 모드
 */

#pragma once

#include "Core/CoreTypes.h"

class AActor;
class UCameraComponent;

struct FCameraDOFSettings;
struct FCameraFocusSettings;
struct FSceneDepthDesc;

/** CameraDepth 디버그 표시 모드 */
enum class ECameraDepthDebugView : uint8
{
    CDDV_None,       // 디버그 표시 없음
    CDDV_SceneDepth, // SceneDepth 시각화
    CDDV_FocusPlane, // Focus Plane 시각화
    CDDV_BlurAmount  // DOF Blur Amount 시각화
};
