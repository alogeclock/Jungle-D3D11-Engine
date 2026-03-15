#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

UPrimitiveComponent::~UPrimitiveComponent() {}

void UPrimitiveComponent::Render(URenderer &renderer)
{
    FConstants constants;
    constants.worldMatrix = Transform.ToMatrix();
    renderer.RenderPrimitive(this, constants);
}