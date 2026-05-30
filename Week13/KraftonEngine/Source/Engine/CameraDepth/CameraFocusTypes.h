/**
 * @file CameraFocusTypes.h
 * @brief Camera Focus 계산에 필요한 공용 타입 정의.
 *
 * 포함 타입:
 * - ECameraFocusMode: FocusDistance 계산 방식
 * - FCameraFocusSettings: Camera 초점 설정
 */

#pragma once

#include "CameraDOFSettings.h"
#include "Core/UObject/TSoftObjectPtr.h"
#include "GameFramework/AActor.h"
#include "CameraFocusTypes.generated.h"
#include "Object/ObjectMacros.h"

/** Camera Focus 계산 방식 */
UENUM()
enum class ECameraFocusMode : uint8
{
    CFM_Manual,        // 수동 FocusDistance 사용
    CFM_TrackingActor, // 특정 Actor까지의 거리 사용
    CFM_ScreenCenter,   // 화면 중앙 Depth 사용
    CFM_None   // 화면 중앙 Depth 사용
};

/** Camera 초점 설정 */
USTRUCT()
struct FCameraFocusSettings
{
    GENERATED_BODY(FCameraFocusSettings)

	UPROPERTY(Edit, Category = "Focus", DisplayName = "FocusMode")
    // Runtime supports only manual distance in the editor for now.
    ECameraFocusMode FocusMode = ECameraFocusMode::CFM_Manual; // FocusDistance 계산 방식

    UPROPERTY(Edit, Category="Focus", DisplayName="Manual Focus Distance", Min=0.01f, Max=100000.0f, Speed=1.0f)
    float ManualFocusDistance = 10.0f; // 수동 초점 거리

	UPROPERTY(Edit, Category = "Focus", DisplayName = "TrackingTarget", Type=SoftObject, Class=AActor)
	TSoftObjectPtr<AActor> TrackingTarget = nullptr; // 추적할 Actor

    float FocusInterpSpeed = 8.0f; // FocusDistance 보간 속도
private:
	//  유효한 내부 상태값
	float CurrentFocusDistance = 10.0f; // 현재 보간된 FocusDistance
};
