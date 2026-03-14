#pragma once

#include "Engine/Source/Runtime/Editor/Public/Application.h"
#include "Engine/Source/Runtime/Core/Public/Math/Matrix.h"
#include "Engine/Source/Runtime/Core/Public/Math/Vector.h"

//#include "Engine/Source/Runtime/Editor/Public/Viewport.h"

// 사용자의 Input 이벤트, 카메라 관리

// TODO: 오브젝트 선택, 기즈모 드래그, WSAD, 우클릭 카메라 조정

class FViewport;

struct FKey
{
	uint32 KeyCode;
	explicit FKey(uint32 InCode) : KeyCode(InCode) {}
	bool operator==(const FKey& O) const { return KeyCode == O.KeyCode; }
};

namespace EKeys
{
	inline const FKey W{ 'W' };
	inline const FKey A{ 'A' };
	inline const FKey S{ 'S' };
	inline const FKey D{ 'D' };
	inline const FKey LeftMouseButton{ VK_LBUTTON };
	inline const FKey RightMouseButton{ VK_RBUTTON };
}

enum class EInputEvent : uint8
{
	Pressed	 = 0,
	Released = 1,
	Repeat	 = 2,
	Axis	 = 3,
};

struct FInputEventState
{
public:
	FInputEventState(FViewport* InViewport, FKey InKey, EInputEvent InInputEvent)
		: Viewport(InViewport)
		, Key(InKey)
		, InputEvent(InInputEvent)
	{
	} 

	FViewport* GetViewport() const { return Viewport; }
	EInputEvent GetInputEvent() const { return InputEvent; }
	FKey GetKey() const { return Key; }

	bool IsLeftMouseButtonPressed() const;
	bool IsRightMouseButtonPressed() const;
	bool IsButtonPressed(FKey InKey) const;

private:
	/** Viewport the event was sent to */
	FViewport* Viewport;
	/** Pressed Key */
	FKey Key;
	/** Key event */
	EInputEvent InputEvent;
};

/**
 * Stores the transformation data for the viewport camera
 */
struct FViewportCameraTransform
{
public:
	FViewportCameraTransform();

	/** Sets the transform's location */
	void SetLocation(const FVector<float>& Position);

	/** Sets the transform's rotation */
	void SetRotation(const FVector<float>& Rotation)
	{
		ViewRotation = Rotation;
	}

	/** Sets the location to look at during orbit */
	void SetLookAt(const FVector<float>& InLookAt)
	{
		LookAt = InLookAt;
	}

	/** Set the ortho zoom amount */
	void SetOrthoZoom(float InOrthoZoom)
	{
		if (InOrthoZoom >= Max_OrthoZoom && InOrthoZoom <= Min_OrthoZoom) return;

		OrthoZoom = InOrthoZoom;
	}

	void SetMaxLocation(double InMaxLocation)
	{
		MaxLocation = InMaxLocation;
	}

	/** Check if transition curve is playing. */
	bool IsPlaying();

	/** @return The transform's location */
	inline const FVector<float>& GetLocation() const { return ViewLocation; }

	/** @return The transform's rotation */
	inline const FVector<float>& GetRotation() const { return ViewRotation; }

	/** @return The look at point for orbiting */
	inline const FVector<float>& GetLookAt() const { return LookAt; }

	/** @return The ortho zoom amount */
	inline float GetOrthoZoom() const { return OrthoZoom; }
	/**
	 * Updates any current location transitions
	 *
	 * @return true if there is currently a transition
	 */
	bool UpdateTransition();

	/**
	 * Computes a matrix to use for viewport location and rotation when orbiting
	 */
	FMatrix<float> ComputeOrbitMatrix() const;


private:
	/** Current viewport Position. */
	FVector<float>	ViewLocation;
	/** Current Viewport orientation; valid only for perspective projections. */
	FVector<float> ViewRotation;
	/** Desired viewport location when animating between two locations */
	FVector<float>	DesiredLocation;
	/** When orbiting, the point we are looking at */
	FVector<float> LookAt;
	/** Viewport start location when animating to another location */
	FVector<float> StartLocation;
	/** Ortho zoom amount */
	float OrthoZoom = 1.0f;
	float Max_OrthoZoom = 1000.0f;
	float Min_OrthoZoom = 1.0f;
	/** Location is clamped to a box around the origin with this radius */
	double MaxLocation = 1000.0f;
};

class FEditorViewportClient
{
public:
	FEditorViewportClient() = default;
	~FEditorViewportClient() = default;

public:
	// FViewport → 매 프레임 호출
	void Tick(float DeltaTime, FViewport* Viewport);

	bool InputKey(const FInputEventState& InputState);
	void MouseMove(FViewport* Viewport, int32 X, int32 Y);
	void InputAxis(FViewport* Viewport, FKey Key, float Delta, float DeltaTime);

	// 현재 카메라 View Matrix — Object에 전달하여 행렬곱
	FMatrix<float> GetViewMatrix() const;

	// 카메라 트랜스폼 직접 접근 (필요 시)
	const FViewportCameraTransform& GetCameraTransform() const { return CameraTransform; }

private:
	// 카메라 이동 속도
	static constexpr float MoveSpeed = 2.0f;  // units/sec
	static constexpr float RotSpeed = 0.1f;    // deg/pixel
	static constexpr float ZoomSpeed = 10.0f;

	FViewportCameraTransform CameraTransform;

	// 우클릭 드래그 상태
	bool  bRightMouseDragging = false;
	int32 LastMouseX = 0;
	int32 LastMouseY = 0;

	// WASD 이동 누적
	void ApplyMovement(float DeltaTime, FViewport* Viewport);

};
