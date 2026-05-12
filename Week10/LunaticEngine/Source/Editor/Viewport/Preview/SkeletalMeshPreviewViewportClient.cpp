#include "SkeletalMeshPreviewViewportClient.h"

#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/RayTypes.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputRouter.h"
#include "Engine/Input/InputSystem.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Math/Quat.h"
#include "Mesh/SkeletalMesh.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

#include <cfloat>
#include <cmath>

namespace
{
	bool GetSkeletalMeshBounds(USkeletalMesh* SkeletalMesh, FVector& OutCenter, FVector& OutExtent);
	float DistancePointToSegment2D(const FVector2& Point, const FVector2& SegmentStart, const FVector2& SegmentEnd);
	bool ProjectWorldToViewport(const FMatrix& ViewProjection, const FVector& WorldPosition, float ViewportWidth, float ViewportHeight, FVector2& OutScreenPosition, float& OutDepth);
	bool TryConvertMouseToViewportPixel(const ImVec2& MousePos, const FRect& ViewportRect, const FViewport* Viewport, float FallbackWidth, float FallbackHeight, float& OutX, float& OutY);
	FQuat GetStableWorldRotation(const USceneComponent* Component);
	FQuat GetComponentSpaceRotation(const USceneComponent* WorldComponent, const USceneComponent* ComponentSpaceOwner);
	FTransform TransformFromMatrix(const FMatrix& Matrix);
	EGizmoMode GizmoModeFromPreviewMode(int32 PreviewMode);
	int32 PreviewModeFromGizmoMode(EGizmoMode Mode);
}

FSkeletalMeshPreviewViewportClient::FSkeletalMeshPreviewViewportClient()
{
}

FSkeletalMeshPreviewViewportClient::~FSkeletalMeshPreviewViewportClient()
{
	DestroyPreviewComponent();
}

void FSkeletalMeshPreviewViewportClient::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	PreviewSkeletalMesh = InSkeletalMesh;
	if (!PreviewSkeletalMesh)
	{
		DestroyPreviewComponent();
		return;
	}

	CreatePreviewComponent();
	if (PreviewComponent)
	{
		PreviewComponent->SetSkeletalMesh(PreviewSkeletalMesh);
	}

	FocusPreviewMesh();
}

void FSkeletalMeshPreviewViewportClient::SetPreviewGizmoMode(int32 InMode)
{
	if (!PreviewGizmo)
	{
		return;
	}

	PreviewGizmo->UpdateGizmoMode(GizmoModeFromPreviewMode(InMode));
}

int32 FSkeletalMeshPreviewViewportClient::GetPreviewGizmoMode() const
{
	return PreviewGizmo ? PreviewModeFromGizmoMode(PreviewGizmo->GetMode()) : 0;
}

void FSkeletalMeshPreviewViewportClient::Tick(float DeltaTime)
{
	FPreviewViewportClient::Tick(DeltaTime);
	SyncPreviewGizmoToSelectedBone();
}

bool FSkeletalMeshPreviewViewportClient::HandleInputSnapshot(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	if (HandlePreviewGizmoInput(Snapshot, DeltaTime))
	{
		return true;
	}

	return FPreviewViewportClient::HandleInputSnapshot(Snapshot, DeltaTime);
}

bool FSkeletalMeshPreviewViewportClient::FocusPreviewMesh()
{
	FVector Center = FVector::ZeroVector;
	FVector Extent = FVector(1.0f, 1.0f, 1.0f);
	if (!GetSkeletalMeshBounds(PreviewSkeletalMesh, Center, Extent))
	{
		return FocusBounds(Center, Extent);
	}

	return FocusBounds(Center, Extent);
}

int32 FSkeletalMeshPreviewViewportClient::PickBoneAtViewportPosition(float LocalX, float LocalY, float ViewportWidth, float ViewportHeight) const
{
	if (!Camera || !PreviewComponent || !PreviewSkeletalMesh || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
	{
		return -1;
	}

	const FSkeletalMesh* MeshAsset = PreviewSkeletalMesh->GetSkeletalMeshAsset();
	const FSkeleton* Skeleton = MeshAsset ? &MeshAsset->Skeleton : nullptr;
	if (!Skeleton || Skeleton->Bones.empty())
	{
		return -1;
	}

	const FMatrix ViewProjection = Camera->GetViewProjectionMatrix();
	const FMatrix& ComponentWorld = PreviewComponent->GetWorldMatrix();
	const FVector2 PickPosition(LocalX, LocalY);
	float BestDistance = FLT_MAX;
	int32 BestBoneIndex = -1;

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton->Bones.size()); ++BoneIndex)
	{
		const FTransform* BoneTransform = PreviewComponent->GetBoneComponentSpaceTransform(BoneIndex);
		if (!BoneTransform)
		{
			continue;
		}

		FVector SegmentStart = ComponentWorld.TransformPositionWithW(BoneTransform->ToMatrix().GetLocation());
		FVector SegmentEnd = SegmentStart;
		const int32 ParentIndex = Skeleton->Bones[BoneIndex].ParentIndex;
		const FTransform* ParentTransform = PreviewComponent->GetBoneComponentSpaceTransform(ParentIndex);
		if (ParentTransform)
		{
			SegmentStart = ComponentWorld.TransformPositionWithW(ParentTransform->ToMatrix().GetLocation());
		}
		else
		{
			const FVector AxisNudge = BoneTransform->Rotation.GetForwardVector() * 2.5f;
			SegmentStart = SegmentStart - AxisNudge;
		}

		FVector2 ScreenStart;
		FVector2 ScreenEnd;
		float StartDepth = 0.0f;
		float EndDepth = 0.0f;
		if (!ProjectWorldToViewport(ViewProjection, SegmentStart, ViewportWidth, ViewportHeight, ScreenStart, StartDepth) ||
			!ProjectWorldToViewport(ViewProjection, SegmentEnd, ViewportWidth, ViewportHeight, ScreenEnd, EndDepth))
		{
			continue;
		}

		const float Distance = DistancePointToSegment2D(PickPosition, ScreenStart, ScreenEnd);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestBoneIndex = BoneIndex;
		}
	}

	const float PickThreshold = 8.0f;
	return BestDistance <= PickThreshold ? BestBoneIndex : -1;
}

bool FSkeletalMeshPreviewViewportClient::IsPreviewGizmoHitAtViewportPosition(float LocalX, float LocalY, float ViewportWidth, float ViewportHeight) const
{
	if (!Camera || !PreviewGizmo || !PreviewGizmo->HasTarget() || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
	{
		return false;
	}

	const FRay Ray = Camera->DeprojectScreenToWorld(LocalX, LocalY, ViewportWidth, ViewportHeight);
	FRayHitResult HitResult;
	return PreviewGizmo->LineTraceComponent(Ray, HitResult);
}

void FSkeletalMeshPreviewViewportClient::OnCameraReset()
{
	FocusPreviewMesh();
}

void FSkeletalMeshPreviewViewportClient::CreatePreviewComponent()
{
	if (PreviewComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	PreviewActor = GetPreviewScene().SpawnPreviewActor();
	if (!PreviewActor)
	{
		return;
	}

	PreviewComponent = PreviewActor->AddComponent<USkeletalMeshComponent>();
	PreviewComponent->SetDisplayBones(true);
	PreviewActor->SetRootComponent(PreviewComponent);
	CreatePreviewGizmo();
}

void FSkeletalMeshPreviewViewportClient::DestroyPreviewComponent()
{
	DestroyPreviewGizmo();

	if (PreviewComponent)
	{
		PreviewComponent->DestroyRenderState();
	}

	if (PreviewActor)
	{
		GetPreviewScene().DestroyPreviewActor(PreviewActor);
	}

	PreviewActor = nullptr;
	PreviewComponent = nullptr;
}

void FSkeletalMeshPreviewViewportClient::CreatePreviewGizmo()
{
	if (!PreviewActor || PreviewGizmo)
	{
		return;
	}

	PreviewGizmoTarget = PreviewActor->AddComponent<USceneComponent>();
	PreviewGizmo = PreviewActor->AddComponent<UGizmoComponent>();

	// 기즈모 컴포넌트들이 스켈레탈 메시(Root)의 자식이 되지 않도록 부모 관계를 해제합니다.
	// 메시의 비균등 축척이 기즈모의 회전 행렬 분해에 영향을 주어 드리프트가 발생하는 것을 방지합니다.
	PreviewGizmoTarget->SetParent(nullptr);
	PreviewGizmo->SetParent(nullptr);

	PreviewGizmo->SetTarget(PreviewGizmoTarget);
	PreviewGizmo->Deactivate();
	PreviewGizmoBoneIndex = -1;
}

void FSkeletalMeshPreviewViewportClient::DestroyPreviewGizmo()
{
	if (PreviewGizmo)
	{
		PreviewGizmo->Deactivate();
		PreviewGizmo->DestroyRenderState();
	}

	PreviewGizmo = nullptr;
	PreviewGizmoTarget = nullptr;
	PreviewGizmoBoneIndex = -1;
}

void FSkeletalMeshPreviewViewportClient::SyncPreviewGizmoToSelectedBone()
{
	if (!PreviewGizmo || !PreviewGizmoTarget || !PreviewComponent || !Camera)
	{
		return;
	}

	const int32 SelectedBoneIndex = PreviewComponent->GetSelectedBoneIndex();
	const FTransform* BoneComponentTransform = PreviewComponent->GetBoneComponentSpaceTransform(SelectedBoneIndex);
	const bool bShowGizmo = GetRenderOptions().ShowFlags.bGizmo && BoneComponentTransform != nullptr;
	if (!bShowGizmo)
	{
		PreviewGizmo->Deactivate();
		PreviewGizmoBoneIndex = -1;
		ResetPreviewGizmoRotationDragState();
		return;
	}

	if (PreviewGizmo->IsHolding())
	{
		const FEditorSettings& Settings = FEditorSettings::Get();
		const bool bForceLocalForScale = PreviewGizmo->GetMode() == EGizmoMode::Scale;
		PreviewGizmo->SetWorldSpace(bForceLocalForScale ? false : (Settings.CoordSystem == EEditorCoordSystem::World));
		PreviewGizmo->SetSnapSettings(
			Settings.bEnableTranslationSnap, Settings.TranslationSnapSize,
			Settings.bEnableRotationSnap, Settings.RotationSnapSize,
			Settings.bEnableScaleSnap, Settings.ScaleSnapSize);
		PreviewGizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(GetRenderOptions().ViewportType, PreviewGizmo->GetMode()));
		PreviewGizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(), Camera->IsOrthogonal(), Camera->GetOrthoWidth());
		ApplyPreviewGizmoToSelectedBone();
		return;
	}

	const FMatrix MeshWorldMatrix = PreviewComponent->GetWorldMatrix();
	const FQuat MeshWorldRotation = GetStableWorldRotation(PreviewComponent);

	// WorldLocation = MeshWorldMatrix * BoneLocalLocation
	const FVector BoneWorldLocation = MeshWorldMatrix.TransformPositionWithW(BoneComponentTransform->Location);
	// WorldRotation = MeshWorldRotation * BoneLocalRotation (Parent * Relative)
	const FQuat BoneWorldRotation = (MeshWorldRotation * BoneComponentTransform->Rotation).GetNormalized();

	PreviewGizmoTarget->SetWorldLocation(BoneWorldLocation);
	PreviewGizmoTarget->SetWorldRotation(BoneWorldRotation);
	PreviewGizmoTarget->SetRelativeScale(BoneComponentTransform->Scale);
	PreviewGizmo->SetTarget(PreviewGizmoTarget);

	const FEditorSettings& Settings = FEditorSettings::Get();
	const bool bForceLocalForScale = PreviewGizmo->GetMode() == EGizmoMode::Scale;
	PreviewGizmo->SetWorldSpace(bForceLocalForScale ? false : (Settings.CoordSystem == EEditorCoordSystem::World));
	PreviewGizmo->SetSnapSettings(
		Settings.bEnableTranslationSnap, Settings.TranslationSnapSize,
		Settings.bEnableRotationSnap, Settings.RotationSnapSize,
		Settings.bEnableScaleSnap, Settings.ScaleSnapSize);
	PreviewGizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(GetRenderOptions().ViewportType, PreviewGizmo->GetMode()));
	PreviewGizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(), Camera->IsOrthogonal(), Camera->GetOrthoWidth());
	PreviewGizmo->UpdateGizmoTransform();
	PreviewGizmoBoneIndex = SelectedBoneIndex;
}

void FSkeletalMeshPreviewViewportClient::ApplyPreviewGizmoToSelectedBone()
{
	if (!PreviewGizmoTarget || !PreviewComponent || PreviewGizmoBoneIndex < 0)
	{
		return;
	}

	if (PreviewGizmo && PreviewGizmo->GetMode() == EGizmoMode::Rotate)
	{
		const FTransform* BoneComponentTransform = PreviewComponent->GetBoneComponentSpaceTransform(PreviewGizmoBoneIndex);
		if (!BoneComponentTransform)
		{
			ResetPreviewGizmoRotationDragState();
			return;
		}

		const FQuat CurrentGizmoRotation = GetComponentSpaceRotation(PreviewGizmoTarget, PreviewComponent);
		if (!bPreviewGizmoRotationDragInitialized || PreviewGizmoRotationDragBoneIndex != PreviewGizmoBoneIndex)
		{
			PreviewGizmoRotationDragBoneIndex = PreviewGizmoBoneIndex;
			PreviewGizmoDragStartRotation = CurrentGizmoRotation;
			PreviewBoneDragStartComponentRotation = BoneComponentTransform->Rotation.GetNormalized();
			bPreviewGizmoRotationDragInitialized = true;
		}

		FQuat DeltaRotation = FQuat::Identity;
		FQuat BoneComponentRotation = FQuat::Identity;
		if (PreviewGizmo->IsWorldSpace())
		{
			DeltaRotation = (CurrentGizmoRotation * PreviewGizmoDragStartRotation.Inverse()).GetNormalized();
			BoneComponentRotation = (DeltaRotation * PreviewBoneDragStartComponentRotation).GetNormalized();
		}
		else
		{
			DeltaRotation = (PreviewGizmoDragStartRotation.Inverse() * CurrentGizmoRotation).GetNormalized();
			BoneComponentRotation = (PreviewBoneDragStartComponentRotation * DeltaRotation).GetNormalized();
		}

		PreviewComponent->SetBoneComponentSpaceRotation(PreviewGizmoBoneIndex, BoneComponentRotation);
		return;
	}

	if (PreviewGizmo && PreviewGizmo->GetMode() == EGizmoMode::Scale)
	{
		const FTransform* BoneLocalTransform = PreviewComponent->GetBoneLocalTransform(PreviewGizmoBoneIndex);
		if (!BoneLocalTransform)
		{
			ResetPreviewGizmoRotationDragState();
			return;
		}

		if (!bPreviewGizmoRotationDragInitialized || PreviewGizmoRotationDragBoneIndex != PreviewGizmoBoneIndex)
		{
			PreviewGizmoRotationDragBoneIndex = PreviewGizmoBoneIndex;
			PreviewBoneDragStartLocalScale = BoneLocalTransform->Scale;
			PreviewGizmoTarget->SetRelativeScale(FVector(1, 1, 1));
			bPreviewGizmoRotationDragInitialized = true;
		}

		// 기즈모의 로컬 스케일을 본의 로컬 스케일에 직접 곱하여 적용합니다.
		// 행렬 분해 과정을 건너뛰어 비균등 부모 스케일 환경에서도 회전값 뒤틀림을 방지합니다.
		FTransform UpdatedLocalTransform = *BoneLocalTransform;
		UpdatedLocalTransform.Scale = PreviewBoneDragStartLocalScale * PreviewGizmoTarget->GetRelativeScale();
		PreviewComponent->SetBoneLocalTransform(PreviewGizmoBoneIndex, UpdatedLocalTransform);
		return;
	}

	ResetPreviewGizmoRotationDragState();
	
	const FMatrix BoneComponentMatrix = PreviewGizmoTarget->GetWorldMatrix() * PreviewComponent->GetWorldInverseMatrix();
	const FVector NewLocation = BoneComponentMatrix.GetLocation();
	const FVector NewScale = BoneComponentMatrix.GetScale();

	// 회전값은 보존하고 위치와 스케일만 업데이트하여 행렬 분해로 인한 회전값 드리프트를 방지합니다.
	const FTransform* CurrentTransform = PreviewComponent->GetBoneComponentSpaceTransform(PreviewGizmoBoneIndex);
	if (CurrentTransform)
	{
		FTransform UpdatedTransform = *CurrentTransform;
		UpdatedTransform.Location = NewLocation;
		UpdatedTransform.Scale = NewScale;
		PreviewComponent->SetBoneComponentSpaceTransform(PreviewGizmoBoneIndex, UpdatedTransform);
	}
}

void FSkeletalMeshPreviewViewportClient::ResetPreviewGizmoRotationDragState()
{
	PreviewGizmoRotationDragBoneIndex = -1;
	PreviewGizmoDragStartRotation = FQuat::Identity;
	PreviewBoneDragStartComponentRotation = FQuat::Identity;
	bPreviewGizmoRotationDragInitialized = false;
}

// 마우스 좌표를 뷰포트 좌표로 변환한 뒤, Raycasting을 수행하고 마우스 조작에 따라 기즈모 조작을 적절하게 처리합니다.
bool FSkeletalMeshPreviewViewportClient::HandlePreviewGizmoInput(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	(void)DeltaTime;
	SyncPreviewGizmoToSelectedBone();

	if (!Camera || !PreviewGizmo || !PreviewGizmo->HasTarget())
	{
		return false;
	}

	const float ViewportWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
	const float ViewportHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
	float LocalMouseX = 0.0f;
	float LocalMouseY = 0.0f;
	if (!TryConvertMouseToViewportPixel(ImGui::GetIO().MousePos, GetViewportScreenRect(), Viewport, WindowWidth, WindowHeight, LocalMouseX, LocalMouseY))
	{
		return PreviewGizmo->IsHolding();
	}

	const FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, ViewportWidth, ViewportHeight);
	FRayHitResult HitResult;
	const bool bGizmoHit = PreviewGizmo->LineTraceComponent(Ray, HitResult);

	if (Snapshot.IsMouseButtonPressed(FInputManager::MOUSE_LEFT) && bIsHovered)
	{
		if (bGizmoHit)
		{
			FInputRouter::Get().SetMouseCapturedViewport(this);
			PreviewGizmo->SetPressedOnHandle(true);
			return true;
		}
	}
	else if (Snapshot.IsMouseButtonDown(FInputManager::MOUSE_LEFT))
	{
		if (PreviewGizmo->IsPressedOnHandle() && !PreviewGizmo->IsHolding())
		{
			PreviewGizmo->SetHolding(true);
		}
		if (PreviewGizmo->IsHolding())
		{
			PreviewGizmo->UpdateDrag(Ray);
			ApplyPreviewGizmoToSelectedBone();
			return true;
		}
	}
	else if (Snapshot.IsMouseButtonReleased(FInputManager::MOUSE_LEFT))
	{
		if (PreviewGizmo->IsHolding() || PreviewGizmo->IsPressedOnHandle())
		{
			PreviewGizmo->DragEnd();
			ApplyPreviewGizmoToSelectedBone();
			ResetPreviewGizmoRotationDragState();
			if (!Snapshot.IsMouseButtonDown(VK_RBUTTON))
			{
				FInputRouter::Get().ReleaseMouseCapture(this);
			}
			return true;
		}
	}

	return false;
}

namespace
{
	bool GetSkeletalMeshBounds(USkeletalMesh* SkeletalMesh, FVector& OutCenter, FVector& OutExtent)
	{
		if (!SkeletalMesh)
		{
			return false;
		}

		const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
		if (!MeshAsset)
		{
			return false;
		}

		const FSkeletalMeshLOD* LOD = MeshAsset->GetLOD(0);
		if (!LOD || !LOD->bBoundsValid)
		{
			return false;
		}

		OutCenter = LOD->BoundsCenter;
		OutExtent = LOD->BoundsExtent;
		return true;
	}

	float DistancePointToSegment2D(const FVector2& Point, const FVector2& SegmentStart, const FVector2& SegmentEnd)
	{
		const FVector2 Segment = SegmentEnd - SegmentStart;
		const float SegmentLengthSq = Segment.Dot(Segment);
		if (SegmentLengthSq <= 1.e-6f)
		{
			return (Point - SegmentStart).Length();
		}

		const float T = Clamp((Point - SegmentStart).Dot(Segment) / SegmentLengthSq, 0.0f, 1.0f);
		const FVector2 Closest = SegmentStart + Segment * T;
		return (Point - Closest).Length();
	}

	// 
	bool ProjectWorldToViewport(const FMatrix& ViewProjection, const FVector& WorldPosition, float ViewportWidth, float ViewportHeight, FVector2& OutScreenPosition, float& OutDepth)
	{
		const FVector ClipSpace = ViewProjection.TransformPositionWithW(WorldPosition);
		OutScreenPosition.X = (ClipSpace.X * 0.5f + 0.5f) * ViewportWidth;
		OutScreenPosition.Y = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * ViewportHeight;
		OutDepth = ClipSpace.Z;
		return OutDepth >= 0.0f && OutDepth <= 1.0f &&
			std::isfinite(OutScreenPosition.X) && std::isfinite(OutScreenPosition.Y) && std::isfinite(OutDepth);
	}

	// 마우스 위치와 뷰포트의 위치 정보를 계산해서 마우스 위치가 뷰포트의 어느 위치에 존재하는지 계산합니다.
	bool TryConvertMouseToViewportPixel(const ImVec2& MousePos, const FRect& ViewportRect, const FViewport* Viewport, float FallbackWidth, float FallbackHeight, float& OutX, float& OutY)
	{
		if (ViewportRect.Width <= 0.0f || ViewportRect.Height <= 0.0f)
		{
			return false;
		}

		const float LocalX = MousePos.x - ViewportRect.X;
		const float LocalY = MousePos.y - ViewportRect.Y;

		const float TargetWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : FallbackWidth;
		const float TargetHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : FallbackHeight;
		if (TargetWidth <= 0.0f || TargetHeight <= 0.0f)
		{
			return false;
		}

		OutX = LocalX * (TargetWidth / ViewportRect.Width);
		OutY = LocalY * (TargetHeight / ViewportRect.Height);
		return true;
	}

	// 부모-자식 계층 구조를 가진 컴포넌트의 최종 월드 공간 기준 회전값을 재귀적으로 구합니다.
	FQuat GetStableWorldRotation(const USceneComponent* Component)
	{
		if (!Component)
		{
			return FQuat::Identity;
		}

		FQuat RelativeRotation = Component->GetRelativeQuat();
		if (const USceneComponent* Parent = Component->GetParent())
		{
			// World = ParentWorld * RelativeRotation
			return (GetStableWorldRotation(Parent) * RelativeRotation).GetNormalized();
		}
		return RelativeRotation.GetNormalized();
	}

	// 메쉬의 발 밑 또는 골반(Root)을 기준으로 한 회전값을 측정합니다.
	FQuat GetComponentSpaceRotation(const USceneComponent* WorldComponent, const USceneComponent* ComponentSpaceOwner)
	{
		const FQuat WorldRotation = GetStableWorldRotation(WorldComponent);
		if (!ComponentSpaceOwner)
		{
			return WorldRotation;
		}

		const FQuat ComponentWorldRotation = GetStableWorldRotation(ComponentSpaceOwner);
		// Relative = ComponentWorldInverse * World. Since A * B means B first, ComponentWorldInverse must be on the left.
		return (ComponentWorldRotation.Inverse() * WorldRotation).GetNormalized();
	}

	// Matrix에서 FTransform 정보를 추출하되, 회전값은 Quaternion으로 변환한 뒤 추출합니다.
	FTransform TransformFromMatrix(const FMatrix& Matrix)
	{
		FQuat Rotation = Matrix.ToQuat();
		Rotation.Normalize();
		return FTransform(Matrix.GetLocation(), Rotation, Matrix.GetScale());
	}

	// int32 PreviewMode 값을 EGizmoMode Enum 값으로 변환합니다. 
	EGizmoMode GizmoModeFromPreviewMode(int32 PreviewMode)
	{
		switch (PreviewMode)
		{
		case 1:
			return EGizmoMode::Rotate;
		case 2:
			return EGizmoMode::Scale;
		default:
			return EGizmoMode::Translate;
		}
	}

	// EGizmoMode Enum 값을 int32 PreviewMode 값으로 변환합니다.
	int32 PreviewModeFromGizmoMode(EGizmoMode Mode)
	{
		switch (Mode)
		{
		case EGizmoMode::Rotate:
			return 1;
		case EGizmoMode::Scale:
			return 2;
		default:
			return 0;
		}
	}
}
