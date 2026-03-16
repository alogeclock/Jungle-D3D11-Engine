#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/BaseTransformGizmo.h"

ABaseTransformGizmo::ABaseTransformGizmo() : GizmoType(EGizmoHandleType::Translate), ActiveAxis(EGizmoAxis::None), bIsDragging(false), TargetObject(nullptr) {}

ABaseTransformGizmo::~ABaseTransformGizmo() {}

void ABaseTransformGizmo::SetTargetObject(UPrimitiveComponent *InTarget) { TargetObject = InTarget; }

UPrimitiveComponent *ABaseTransformGizmo::GetTargetObject() const { return TargetObject; }