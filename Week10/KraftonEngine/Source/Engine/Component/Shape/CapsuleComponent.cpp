#include "CapsuleComponent.h"
#include "GameFramework/World.h"

IMPLEMENT_CLASS(UCapsuleComponent, UShapeComponent)

void UCapsuleComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Capsule Radius",		EPropertyType::Float, &CapsuleRadius,	  0.0f, 500.0f, 0.1f });
	OutProps.push_back({ "Capsule Half-Height", EPropertyType::Float, &CapsuleHalfHeight, 0.0f, 500.0f, 0.1f });
}

void UCapsuleComponent::DrawDebugShape(FScene& Scene) const {
	if (CapsuleRadius <= 0.f || CapsuleHalfHeight < 0.f) return;

	FVector Center    = GetWorldLocation();
	FVector Up        = GetUpVector().Normalized();
	FVector Fwd       = GetForwardVector().Normalized();
	FVector Right     = GetRightVector().Normalized();
	FVector TopCenter = Center + Up * CapsuleHalfHeight;
	FVector BotCenter = Center - Up * CapsuleHalfHeight;
	FVector NegUp     = Up * -1.f;

	constexpr uint32 Segments     = 24;
	constexpr uint32 HalfSegments = Segments / 2;

	// Junction rings at top and bottom
	DrawDebugRing(TopCenter, CapsuleRadius, Fwd, Right, Segments,     false, Scene);
	DrawDebugRing(BotCenter, CapsuleRadius, Fwd, Right, Segments,     false, Scene);

	// Top hemisphere
	DrawDebugRing(TopCenter, CapsuleRadius, Fwd,  Up,    HalfSegments, true, Scene);
	DrawDebugRing(TopCenter, CapsuleRadius, Right, Up,    HalfSegments, true, Scene);

	// Bottom hemisphere
	DrawDebugRing(BotCenter, CapsuleRadius, Fwd,  NegUp, HalfSegments, true, Scene);
	DrawDebugRing(BotCenter, CapsuleRadius, Right, NegUp, HalfSegments, true, Scene);

	// Body lines
	Scene.AddDebugLine(TopCenter + Fwd   * CapsuleRadius, BotCenter + Fwd   * CapsuleRadius, ShapeColor);
	Scene.AddDebugLine(TopCenter - Fwd   * CapsuleRadius, BotCenter - Fwd   * CapsuleRadius, ShapeColor);
	Scene.AddDebugLine(TopCenter + Right * CapsuleRadius, BotCenter + Right * CapsuleRadius, ShapeColor);
	Scene.AddDebugLine(TopCenter - Right * CapsuleRadius, BotCenter - Right * CapsuleRadius, ShapeColor);
}