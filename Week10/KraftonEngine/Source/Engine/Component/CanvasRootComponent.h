#pragma once

#include "Component/SceneComponent.h"

class UCanvasRootComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UCanvasRootComponent, USceneComponent)

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	const FVector& GetCanvasSize() const { return CanvasSize; }
	void SetCanvasSize(const FVector& InCanvasSize) { CanvasSize = InCanvasSize; }

private:
	FVector CanvasSize = FVector(1920.0f, 1080.0f, 0.0f);
};
