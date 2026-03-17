#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/BaseTransformGizmo.h"

ABaseTransformGizmo::ABaseTransformGizmo() : GizmoType(EGizmoHandleType::Translate), ActiveAxis(EGizmoAxis::None), bIsDragging(false), TargetObject(nullptr) {}

ABaseTransformGizmo::~ABaseTransformGizmo() {}

void ABaseTransformGizmo::SetTargetObject(USceneComponent *InTarget) { TargetObject = InTarget; }

USceneComponent *ABaseTransformGizmo::GetTargetObject() const { return TargetObject; }