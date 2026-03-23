#include "Source/Core/Public/Memory.h"
#include "Source/Editor/Public/BaseTransformGizmo.h"

ABaseTransformGizmo::ABaseTransformGizmo(const FString &InString)
    : AActor(InString), GizmoType(EGizmoHandleType::Translate), ActiveAxis(EGizmoAxis::None), bIsDragging(false)
{
    if (GEditor) GEditor->RegisterInputListener(this);
}

ABaseTransformGizmo::~ABaseTransformGizmo() 
{
}