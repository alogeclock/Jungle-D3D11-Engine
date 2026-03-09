#include "Camera2D.h"

void FCamera2D::Reset()
{
	Position = {};
	TargetCenter = {};
}

void FCamera2D::SetViewportSize(float Width, float Height)
{
	ViewportWidth = Width;
	ViewportHeight = Height;
}

void FCamera2D::SetWorldBounds(float Width, float Height)
{
	WorldWidth = Width;
	WorldHeight = Height;
}

void FCamera2D::SetPosition(float X, float Y)
{
	Position.X = X;
	Position.Y = Y;
}

void FCamera2D::SetTargetCenter(const FVec2& WorldCenter)
{
	TargetCenter = WorldCenter;
}

void FCamera2D::SetZoom(float InZoom)
{
	Zoom = InZoom;
}

float FCamera2D::GetZoom() const
{
	return Zoom;
}

void FCamera2D::SetFollowSpeed(float InFollowSpeed)
{
	FollowSpeed = InFollowSpeed;
}

float FCamera2D::GetFollowSpeed() const
{
	return FollowSpeed;
}

void FCamera2D::Update(float DeltaTime)
{
	float TargetX = TargetCenter.X - (ViewportWidth * 0.5f) / Zoom;
	float TargetY = TargetCenter.Y - (ViewportHeight * 0.5f) / Zoom;

	float Alpha = FollowSpeed * DeltaTime;
	if (Alpha > 1.0f) Alpha = 1.0f;

	Position.X += (TargetX - Position.X) * Alpha;
	Position.Y += (TargetY - Position.Y) * Alpha;

	// Clamp to world bounds
	float MaxX = WorldWidth - ViewportWidth / Zoom;
	float MaxY = WorldHeight - ViewportHeight / Zoom;

	if (Position.X < 0.0f) Position.X = 0.0f;
	if (Position.Y < 0.0f) Position.Y = 0.0f;
	if (MaxX > 0.0f && Position.X > MaxX) Position.X = MaxX;
	if (MaxY > 0.0f && Position.Y > MaxY) Position.Y = MaxY;
}

FVec2 FCamera2D::GetPosition() const
{
	return Position;
}

FVec2 FCamera2D::WorldToScreen(const FVec2& WorldPosition) const
{
	FVec2 Result;
	Result.X = (WorldPosition.X - Position.X) * Zoom;
	Result.Y = (WorldPosition.Y - Position.Y) * Zoom;
	return Result;
}
