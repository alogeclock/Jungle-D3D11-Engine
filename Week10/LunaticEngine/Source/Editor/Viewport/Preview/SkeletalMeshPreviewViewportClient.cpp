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

	const FMatrix BoneWorldMatrix = BoneComponentTransform->ToMatrix() * PreviewComponent->GetWorldMatrix();
	const FTransform BoneWorldTransform = TransformFromMatrix(BoneWorldMatrix);
	PreviewGizmoTarget->SetWorldLocation(BoneWorldTransform.Location);
	PreviewGizmoTarget->SetRelativeRotation(BoneWorldTransform.Rotation);
	PreviewGizmoTarget->SetRelativeScale(BoneWorldTransform.Scale);
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

	const FMatrix BoneComponentMatrix = PreviewGizmoTarget->GetWorldMatrix() * PreviewComponent->GetWorldInverseMatrix();
	PreviewComponent->SetBoneComponentSpaceTransform(PreviewGizmoBoneIndex, TransformFromMatrix(BoneComponentMatrix));
}

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

	bool ProjectWorldToViewport(const FMatrix& ViewProjection, const FVector& WorldPosition, float ViewportWidth, float ViewportHeight, FVector2& OutScreenPosition, float& OutDepth)
	{
		const FVector ClipSpace = ViewProjection.TransformPositionWithW(WorldPosition);
		OutScreenPosition.X = (ClipSpace.X * 0.5f + 0.5f) * ViewportWidth;
		OutScreenPosition.Y = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * ViewportHeight;
		OutDepth = ClipSpace.Z;
		return OutDepth >= 0.0f && OutDepth <= 1.0f &&
			std::isfinite(OutScreenPosition.X) && std::isfinite(OutScreenPosition.Y) && std::isfinite(OutDepth);
	}

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

	FTransform TransformFromMatrix(const FMatrix& Matrix)
	{
		FQuat Rotation = Matrix.ToQuat();
		Rotation.Normalize();
		return FTransform(Matrix.GetLocation(), Rotation, Matrix.GetScale());
	}

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
