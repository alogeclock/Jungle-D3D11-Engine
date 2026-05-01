#pragma once
#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent {
public:
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
	UCapsuleComponent() = default;
	UCapsuleComponent(float InHalfHeight, float InRadius) : CapsuleHalfHeight(InHalfHeight), CapsuleRadius(InRadius) {}

	void  GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
	float GetCapsuleRadius() const { return CapsuleRadius; }
	void  SetCapsuleHalfHeight(float InHalfHeight) { CapsuleHalfHeight = InHalfHeight; }
	void  SetCapsuleRadius(float InRadius) { CapsuleRadius = InRadius; }

	void DrawDebugShape(FScene& Scene) const override;

private:
	float CapsuleHalfHeight = 1.f;
	float CapsuleRadius		= 1.f;
};