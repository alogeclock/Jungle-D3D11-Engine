#pragma once

#include "Object/Object.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"

enum class EGizmoHandleType
{
    Translate,
    Rotate,
    Scale
};

enum class EGizmoAxis
{
    X,
    Y,
    Z,
    None
};

class ABaseTransformGizmo : UObject
{
  public:
    ABaseTransformGizmo();
    virtual ~ABaseTransformGizmo();

    virtual void Update(float DeltaTime) {}
    virtual void Render(URenderer &Renderer) {}

    virtual void         SetTargetObject(UPrimitiveComponent *InTarget);
    UPrimitiveComponent *GetTargetObject() const;

    // 마우스 입력 이벤트 (RayOrigin: 카메라 위치, RayDir: 카메라에서 마우스 커서 방향으로 발사되는 단위 벡터)
    virtual bool OnMouseDown(const FVector<float>& RayOrigin, const FVector<float>& RayDir) = 0;
    virtual void OnMouseMove(const FVector<float>& RayOrigin, const FVector<float>& RayDir) = 0;
    virtual void OnMouseUp() = 0;
    virtual void OnNewObjectsSelected() = 0;

  protected:
    EGizmoHandleType GizmoType = EGizmoHandleType::Translate;
    EGizmoAxis       ActiveAxis = EGizmoAxis::None;
    bool             bIsDragging = false;

    UPrimitiveComponent *TargetObject = nullptr;
};