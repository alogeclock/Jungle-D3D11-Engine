#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ActorComponent.h"
#include "Engine/Source/Runtime/Core/Public/CoreTypes.h"
#include "Engine/Source/Runtime/Core/Public/Math/Transform.h"

// Transform Information을 갖는 Component (위치, 회전, 크기)
class USceneComponent : public UActorComponent
{
  public:
    USceneComponent();
    virtual ~USceneComponent();

    // transform 관련 접근자
    void           SetLocation(const FVector<float> &NewLocation);
    FVector<float> GetLocation() const;

    void           SetRotation(const FVector<float> &NewRotation);
    FVector<float> GetRotation() const;

    void           SetScale(const FVector<float> &NewScale);
    FVector<float> GetScale() const;

    void SetTransform(const FTransform &InTransform);
    FTransform GetTransform() const;

    // SRT 행렬을 Update하는 함수
    void UpdateWorldMatrix();
    void UpdateWorldMatrix(const FTransform &InTransform);
    const FMatrix<float> &GetWorldMatrix() const;

  protected:
    FTransform Transform;

    FMatrix<float> WorldMatrix;
};