#pragma once

#include "../Core/Types.h"

class FCamera2D
{
public:
	void Reset();

	void SetViewportSize(float Width, float Height);
	void SetWorldBounds(float Width, float Height);

	void SetPosition(float X, float Y);
	void SetTargetCenter(const FVec2& WorldCenter);

	void SetZoom(float InZoom);
	float GetZoom() const;

	void SetFollowSpeed(float InFollowSpeed);
	float GetFollowSpeed() const;

	void Update(float DeltaTime);

	FVec2 GetPosition() const;
	FVec2 WorldToScreen(const FVec2& WorldPosition) const;

private:
	FVec2 Position{};
	FVec2 TargetCenter{};

	float ViewportWidth = 0.0f;
	float ViewportHeight = 0.0f;

	float WorldWidth = 0.0f;
	float WorldHeight = 0.0f;

	float Zoom = 1.25f;
	float FollowSpeed = 10.0f;
};
