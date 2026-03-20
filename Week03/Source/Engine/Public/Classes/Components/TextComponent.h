#pragma once
#include "PrimitiveComponent.h"
#include "Source/Core/Public/Math/Matrix.h"

class UTextComponent :
    public UPrimitiveComponent
{
	DECLARE_OBJECT(UTextComponent, UPrimitiveComponent)
public:
	UTextComponent(const FString &InString);
	virtual ~UTextComponent() override;

	const FString& GetText() const { return Text; }
	void SetText(const FString& InText) { Text = InText; }

	void UpdateRotationMatrix(const FVector<float>& InCameraForward);

protected:
	FString Text = FString("Text");
	FMatrix<float> RTMatrix;
};

