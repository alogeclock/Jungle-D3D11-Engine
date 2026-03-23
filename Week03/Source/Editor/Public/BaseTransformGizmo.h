#pragma once

#include "Source/Engine/Object/Public/Object.h"
#include "Source/Engine/Object/Public/Actor.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"
#include "Source/Engine/Public/Rendering/Renderer.h"

enum class EGizmoHandleType
{
    Translate = 0,
    Rotate = 1,
    Scale = 2
};

enum class EGizmoAxis
{
    X,
    Y,
    Z,
    None
};

class ABaseTransformGizmo : public AActor
{
  public:
    ABaseTransformGizmo(const FString &InString);
    virtual ~ABaseTransformGizmo();

    virtual void Update(float DeltaTime) {}

    virtual void     SetTargetObject(USceneComponent *InTarget);
    USceneComponent *GetTargetObject() const;

    // 마우스 입력 이벤트 (RayOrigin: 카메라 위치, RayDir: 카메라에서 마우스 커서 방향으로 발사되는 단위 벡터)
    virtual bool OnMouseDown(const FVector<float> &RayOrigin, const FVector<float> &RayDir) = 0;
    virtual void OnMouseMove(const FVector<float> &RayOrigin, const FVector<float> &RayDir) = 0;
    virtual void OnMouseHover(const FVector<float> &RayOrigin, const FVector<float> &RayDir) = 0;
    virtual void OnMouseUp() = 0;

  protected:
    EGizmoHandleType GizmoType = EGizmoHandleType::Translate;
    EGizmoAxis       ActiveAxis = EGizmoAxis::None;
    bool             bIsDragging = false;

    USceneComponent *TargetObject = nullptr;
};