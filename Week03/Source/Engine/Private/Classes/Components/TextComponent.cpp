#include "Source/Engine/Public/Classes/Components/TextComponent.h"

UTextComponent::UTextComponent(const FString& InString) : UPrimitiveComponent(InString) {
	//TODO: PrimitiveType = Text
	PrimitiveType = EPrimitiveType::Text;
}

UTextComponent::~UTextComponent() {}

void UTextComponent::UpdateRotationMatrix(const FVector<float>& InCameraForward)
{
}


