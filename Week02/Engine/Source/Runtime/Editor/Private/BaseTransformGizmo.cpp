#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/BaseTransformGizmo.h"

ABaseTransformGizmo::ABaseTransformGizmo() : GizmoType(EGizmoHandleType::Translate), ActiveAxis(EGizmoAxis::None), bIsDragging(false), TargetObject(nullptr)
{
}

ABaseTransformGizmo::~ABaseTransformGizmo() 
{
}

// 선택된 오브젝트의 루트 컴포넌트와 부모자식 관계를 형성한다.
void ABaseTransformGizmo::SetTargetObject(USceneComponent *InTarget) { TargetObject = InTarget; }

USceneComponent *ABaseTransformGizmo::GetTargetObject() const { return TargetObject; }