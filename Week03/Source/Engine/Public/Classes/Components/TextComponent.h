#pragma once
#include "PrimitiveComponent.h"
#include "Source/Core/Public/Math/Matrix.h"
#include "Source/Core/Public/Math/Vector.h"
#include<iostream>

class UTextComponent :
    public UPrimitiveComponent
{
	DECLARE_OBJECT(UTextComponent, UPrimitiveComponent)
public:
	UTextComponent(const FString &InString);
	virtual ~UTextComponent() override;

	const FString& GetText() const { return Text; }


	//UUID
	void SetText(const uint32 UUID) {
		char buf[64];
		sprintf_s(buf, "UID:%u", UUID);
		Text = FString(buf);
		bMeshDirty = true;
	}

	void SetText(const FString& InText)
	{
		if (Text == InText) return;
		Text = InText;
		bMeshDirty = true;
	}

	void UpdateBillboard(const FVector<float> &InCameraForward, 
		const FVector<float> &InWorldUp = FVector<float>(0, 0, 1));

	virtual void Render(URenderer &renderer) override;
	void RebuildMesh();

protected:

	FString Text = FString("Text");
	FMatrix<float> RTMatrix;
    bool bMeshDirty = true;

	TArray<FTextVertex> TextVertices;
};

