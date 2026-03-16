#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SceneComponent.h"

USceneComponent::USceneComponent()
{
    Transform.Location = FVector<float>(0.0f, 0.0f, 0.0f);
    Transform.Rotation = FVector<float>(0.0f, 0.0f, 0.0f);
    Transform.Scale = FVector<float>(1.0f, 1.0f, 1.0f);
}

USceneComponent::~USceneComponent() {}

void USceneComponent::SetLocation(const FVector<float> &NewLocation)
{
    bIsWorldMatrixDirty = true;
    Transform.Location = NewLocation;
}

FVector<float> USceneComponent::GetLocation() const { return Transform.Location; }

void USceneComponent::SetRotation(const FVector<float> &NewRotation)
{
    bIsWorldMatrixDirty = true;
    Transform.Rotation = NewRotation;
}

FVector<float> USceneComponent::GetRotation() const { return Transform.Rotation; }

void USceneComponent::SetScale(const FVector<float> &NewScale)
{
    bIsWorldMatrixDirty = true;
    Transform.Scale = NewScale;
}

FVector<float> USceneComponent::GetScale() const { return Transform.Scale; }

void USceneComponent::SetColor(const FVector4<float> &NewColor) { Color = NewColor; }

FVector4<float> USceneComponent::GetColor() const { return Color; }

void USceneComponent::SetTransform(const FTransform &InTransform)
{
    bIsWorldMatrixDirty = true;
    Transform = InTransform;
}

FTransform USceneComponent::GetTransform() const { return Transform; }

void USceneComponent::UpdateWorldMatrix() { WorldMatrix = Transform.ToMatrix(); }

void USceneComponent::UpdateWorldMatrix(const FTransform &InTransform)
{
    Transform = InTransform;
    WorldMatrix = Transform.ToMatrix();
}

const FMatrix<float> &USceneComponent::GetWorldMatrix()
{
    if (bIsWorldMatrixDirty)
    {
        UpdateWorldMatrix();
        bIsWorldMatrixDirty = false;
    }

    return WorldMatrix;
};

void USceneComponent::SetParentMatrix(const FMatrix<float>& NewParentMatrix)
{
    ParentMatrix = NewParentMatrix;
}

const FMatrix<float> USceneComponent::GetParentMatrix() const
{
    return ParentMatrix;
}