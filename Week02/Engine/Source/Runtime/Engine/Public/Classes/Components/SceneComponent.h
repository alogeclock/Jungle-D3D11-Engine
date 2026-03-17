#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ActorComponent.h"
#include "CoreTypes.h"
#include "Engine/Source/Runtime/Core/Public/Math/Transform.h"

// Transform Information을 갖는 Component (위치, 회전, 크기)
class USceneComponent : public UActorComponent
{
  public:
    USceneComponent(const FString &InString);
    virtual ~USceneComponent();

    // transform 관련 접근자
    void           SetLocation(const FVector<float> &NewLocation);
    FVector<float> GetLocation() const;

    void           SetRotation(const FVector<float> &NewRotation);
    FVector<float> GetRotation() const;

    void           SetScale(const FVector<float> &NewScale);
    FVector<float> GetScale() const;

    void            SetColor(const FVector4<float> &NewColor);
    FVector4<float> GetColor() const;

    void       SetTransform(const FTransform &InTransform);
    FTransform GetTransform() const;

    void SetParentMatrix(const FMatrix<float> &ParentMatrix);
    const FMatrix<float> GetParentMatrix() const;

    void                             SetupAttachment(USceneComponent *InParent);
    USceneComponent                 *GetAttachParent() const { return AttachParent; }
    const TArray<USceneComponent *> &GetAttachChildren() const { return AttachChildren; }

    // SRT 행렬을 Update하는 함수
    void                  UpdateWorldMatrix();
    void                  UpdateWorldMatrix(const FTransform &InTransform);
    const FMatrix<float> &GetWorldMatrix();
    
    void MarkTransformDirty();

    static UObject *Constructor() { return new USceneComponent("USceneComponentConstructor"); }

    static UClass *StaticClass()
    {
        // 부모를 UPrimitiveComponent::StaticClass() 로 지정
        static UClass s_Class("USceneComponent", UActorComponent::StaticClass(), &USceneComponent::Constructor);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }

  protected:
    FTransform Transform;
    bool       bIsWorldMatrixDirty = true;

    FMatrix<float>  WorldMatrix = FMatrix<float>::Identity();
    FMatrix<float>  ParentMatrix = FMatrix<float>::Identity();
    FVector4<float> Color = {0.0f, 0.0f, 0.0f, 0.0f};

    // 계층 구조 관리를 위한 멤버 변수
    USceneComponent          *AttachParent = nullptr;
    TArray<USceneComponent *> AttachChildren;
};