#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SceneComponent.h"

USceneComponent::USceneComponent()
{
    Transform.Location = FVector<float>(0.0f, 0.0f, 0.0f);
    Transform.Rotation = FVector<float>(0.0f, 0.0f, 0.0f);
    Transform.Scale = FVector<float>(1.0f, 1.0f, 1.0f);
}

USceneComponent::~USceneComponent() {}

void USceneComponent::MarkTransformDirty()
{
    if (bIsWorldMatrixDirty)
        return;

    bIsWorldMatrixDirty = true;

    for (USceneComponent *Child : AttachChildren)
    {
        if (Child != nullptr)
        {
            Child->MarkTransformDirty();
        }
    }
}

void USceneComponent::SetLocation(const FVector<float> &NewLocation)
{
    Transform.Location = NewLocation;
    MarkTransformDirty();
}

FVector<float> USceneComponent::GetLocation() const { return Transform.Location; }

void USceneComponent::SetRotation(const FVector<float> &NewRotation)
{
    Transform.Rotation = NewRotation;
    MarkTransformDirty();
}

FVector<float> USceneComponent::GetRotation() const { return Transform.Rotation; }

void USceneComponent::SetScale(const FVector<float> &NewScale)
{
    Transform.Scale = NewScale;
    MarkTransformDirty();
}

FVector<float> USceneComponent::GetScale() const { return Transform.Scale; }

void USceneComponent::SetColor(const FVector4<float> &NewColor) { Color = NewColor; }

FVector4<float> USceneComponent::GetColor() const { return Color; }

void USceneComponent::SetTransform(const FTransform &InTransform)
{
    Transform = InTransform;
    MarkTransformDirty();
}

FTransform USceneComponent::GetTransform() const { return Transform; }

void USceneComponent::SetParentMatrix(const FMatrix<float> &NewParentMatrix)
{
    ParentMatrix = NewParentMatrix;
    MarkTransformDirty();
}

const FMatrix<float> USceneComponent::GetParentMatrix() const { return ParentMatrix; }

void USceneComponent::SetupAttachment(USceneComponent *InParent)
{
    if (InParent == this || InParent == nullptr)
        return;

    if (AttachParent != nullptr)
    {
        erase(AttachParent->AttachChildren, this);
    }

    AttachParent = InParent;
    AttachParent->AttachChildren.push_back(this);

    MarkTransformDirty();
}

void USceneComponent::UpdateWorldMatrix()
{
    // 1. 부모가 존재할 경우, 부모의 최신 월드 행렬을 자신의 ParentMatrix로 가져옵니다.
    if (AttachParent != nullptr)
    {
        ParentMatrix = AttachParent->GetWorldMatrix();
    }

    // 2. 로컬 Transform과 부모의 행렬을 곱하여 최종 월드 행렬 도출
    WorldMatrix = Transform.ToMatrix() * ParentMatrix;
    bIsWorldMatrixDirty = false;
}

void USceneComponent::UpdateWorldMatrix(const FTransform &InTransform)
{
    SetTransform(InTransform); // 수정했다는 사실을 기록해 자식에게 수정 사실을 전달
    UpdateWorldMatrix();
}

const FMatrix<float> &USceneComponent::GetWorldMatrix()
{
    if (bIsWorldMatrixDirty)
    {
        UpdateWorldMatrix();
    }

    return WorldMatrix;
};
