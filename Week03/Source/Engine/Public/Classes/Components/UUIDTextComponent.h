#pragma once
#include "Source/Engine/Public/Classes/Components/TextComponent.h"

class UUUIDTextComponent : public UTextComponent
{
	DECLARE_OBJECT(UUUIDTextComponent, UTextComponent)

public:
	UUUIDTextComponent(const FString &InString);
    virtual ~UUUIDTextComponent() override;

	void UpdateBillboard(const FVector<float> &InCameraForward, 
		const FVector<float> &InWorldUp = FVector<float>(0, 0, 1)) override;
		
	virtual void Render(URenderer &renderer) override;

    virtual void Submit(const FSceneViewOptions& ViewOptions) override;

private:
	float Zoffset = 2.5f;
};
