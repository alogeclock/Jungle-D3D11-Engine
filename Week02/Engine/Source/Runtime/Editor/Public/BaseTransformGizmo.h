#pragma once

#include "Engine/Source/Runtime/Core/Public/Object.h"

/// <summary>
/// Object Transform Gizmo를 위한 추상 클래스
/// </summary>

enum class EGizmoHandleTypes
{
	All = 0,
	Translate = 1,
	Rotate = 2,
	Scale = 3
};

class ABaseTransformGizmo : UObject
{
public:
	ABaseTransformGizmo();
	virtual ~ABaseTransformGizmo();

public:
	/** Call this when new objects become selected.  This triggers an animation transition. */
	virtual void OnNewObjectsSelected() = 0;

	/** Called by the world interaction system when one of our components is dragged by the user to find out
		what type of interaction to do.  If null is passed in then we'll treat it as dragging the whole object
		(rather than a specific axis/handle) */
	UE_API class UViewportDragOperationComponent* GetInteractionType(UActorComponent* DraggedComponent, TOptional<FTransformGizmoHandlePlacement>& OutHandlePlacement);

	/** Sets the owner */
	UE_API void SetOwnerWorldInteraction(class UViewportWorldInteraction* InWorldInteraction);

	/** Gets the owner */
	UE_API class UViewportWorldInteraction* GetOwnerWorldInteraction() const;

	/** Called by the world interaction system after we've been spawned into the world, to allow
		us to create components and set everything up nicely for the selected objects that we'll be
		used to manipulate */
	virtual void UpdateGizmo(const EGizmoHandleTypes InGizmoType, const ECoordSystem InGizmoCoordinateSpace, const FTransform& InLocalToWorld, const FBox& InLocalBounds, const FVector& InViewLocation, const float InScaleMultiplier, bool bInAllHandlesVisible,
		const bool bInAllowRotationAndScaleHandles, class UActorComponent* DraggingHandle, const TArray<UActorComponent*>& InHoveringOverHandles, const float InGizmoHoverScale, const float InGizmoHoverAnimationDuration) {
	}


	/** Gets the current gizmo handle type */
	UE_API EGizmoHandleTypes GetGizmoType() const;

protected:

	/** Static: Given a bounding box and information about which edge to query, returns the vertex positions of that edge */
	static UE_API void GetBoundingBoxEdge(const FBox& Box, const int32 AxisIndex, const int32 EdgeIndex, FVector& OutVertex0, FVector& OutVertex1);

	/** Updates the visibility of all the handles */
	UE_API void UpdateHandleVisibility(const EGizmoHandleTypes InGizmoType, const ECoordSystem InGizmoCoordinateSpace, bool bInAllHandlesVisible, UActorComponent* DraggingHandle);

	/** Gets if the gizmo shows measurement texts */
	UE_API bool GetShowMeasurementText() const;

	/** Real time that the gizmo was last attached to a selected set of objects.  This is used for animation transitions */
	FTimespan SelectedAtTime;

	/** Scene component root of this actor */
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneComponent;

	/** All gizmo components */
	UPROPERTY()
	TArray< TObjectPtr<class UGizmoHandleGroup> > AllHandleGroups;

	/** Owning object */
	UPROPERTY()
	TObjectPtr<class UViewportWorldInteraction> WorldInteraction;

	/** Current gizmo type */
	EGizmoHandleTypes GizmoType;
};