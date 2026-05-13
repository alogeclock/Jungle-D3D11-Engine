#include "SkeletalMeshPreviewViewportClient.h"

#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/RayTypes.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputRouter.h"
#include "Engine/Input/InputSystem.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/Quat.h"
#include "Mesh/SkeletalMesh.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

#include <cmath>

namespace
{
	// Picking
	bool GetSkeletalMeshBounds(USkeletalMesh* SkeletalMesh, FVector& OutCenter, FVector& OutExtent);
	bool TryConvertMouseToViewportPixel(const ImVec2& MousePos, const FRect& ViewportRect, const FViewport* Viewport, float FallbackWidth, float FallbackHeight, float& OutX, float& OutY);
	
	// Update Transform
	FQuat ExtractRotationQuatNoScale(const FMatrix& Matrix, const FVector* PreferredScaleForMirrorAxis = nullptr);
	FQuat MakeQuatContinuous(const FQuat& Quat, const FQuat& ReferenceQuat);
	FQuat GetStableWorldRotation(const USceneComponent* Component);
	FQuat GetComponentSpaceRotation(const USceneComponent* WorldComponent, const USceneComponent* ComponentSpaceOwner);
	FQuat GetBoneComponentSpaceRotation(const USkeletalMeshComponent* Component, int32 BoneIndex, const FQuat* ReferenceQuat = nullptr);
	FQuat GetBoneWorldRotation(const USkeletalMeshComponent* Component, int32 BoneIndex, const FQuat* ReferenceQuat = nullptr);
	
	// Gizmo
	EGizmoMode GizmoModeFromPreviewMode(int32 PreviewMode);
	int32 PreviewModeFromGizmoMode(EGizmoMode Mode);
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
	const bool bAltLeftNavigation = Snapshot.IsKeyDown(VK_MENU) && (Snapshot.IsMouseButtonDown(FInputManager::MOUSE_LEFT) || Snapshot.IsMouseButtonPressed(
		FInputManager::MOUSE_LEFT
	));

	// Alt + LMB orbit은 스켈레탈 뷰어의 현재 선택 본/프리뷰 메시를 기준으로 돌아야 합니다.
	// 기본 FPreviewViewportClient는 마지막 FocusBounds()에서 잡힌 OrbitTarget을 계속 쓰므로,
	// 본 선택 후에는 orbit 기준점이 메시 중심에 남아 조작이 어긋나 보일 수 있습니다.
	if (bAltLeftNavigation && Snapshot.IsMouseButtonPressed(FInputManager::MOUSE_LEFT))
	{
		UpdateOrbitTargetForAltNavigation();
	}

	if (!bAltLeftNavigation || (PreviewGizmo && PreviewGizmo->IsHolding()))
	{
		if (HandlePreviewGizmoInput(Snapshot, DeltaTime))
		{
			return true;
		}
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

void FSkeletalMeshPreviewViewportClient::UpdateOrbitTargetForAltNavigation()
{
	if (!PreviewComponent)
	{
		return;
	}

	// 본이 선택되어 있으면 Alt orbit의 기준점을 선택 본 위치로 맞춥니다.
	// 화면의 본/메시 스키닝과 동일한 ComponentSpaceMatrix 기준을 사용해야 미러 본에서 좌우가 맞습니다.
	const int32 SelectedBoneIndex = PreviewComponent->GetSelectedBoneIndex();
	FMatrix BoneComponentMatrix = FMatrix::Identity;
	if (PreviewComponent->GetBoneComponentSpaceMatrix(SelectedBoneIndex, BoneComponentMatrix))
	{
		const FVector BoneWorldLocation = PreviewComponent->GetWorldMatrix().TransformPositionWithW(BoneComponentMatrix.GetLocation());
		SetOrbitTarget(BoneWorldLocation);
		return;
	}

	// 선택 본이 없으면 메시 bounds 중심을 world 위치로 변환해서 사용합니다.
	FVector BoundsCenter = FVector::ZeroVector;
	FVector BoundsExtent = FVector(1.0f, 1.0f, 1.0f);
	if (GetSkeletalMeshBounds(PreviewSkeletalMesh, BoundsCenter, BoundsExtent))
	{
		SetOrbitTarget(PreviewComponent->GetWorldMatrix().TransformPositionWithW(BoundsCenter));
		return;
	}

	SetOrbitTarget(PreviewComponent->GetWorldLocation());
}

int32 FSkeletalMeshPreviewViewportClient::PickBoneAtViewportPosition(float LocalX, float LocalY, float ViewportWidth, float ViewportHeight) const
{
	if (!Camera || !PreviewComponent || !PreviewSkeletalMesh || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
	{
		return -1;
	}

	const FRay Ray = Camera->DeprojectScreenToWorld(LocalX, LocalY, ViewportWidth, ViewportHeight);
	return PreviewComponent->PickBoneArmature(Ray);
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
	FMatrix BoneComponentMatrix = FMatrix::Identity;
	const bool bHasBoneComponentMatrix = PreviewComponent->GetBoneComponentSpaceMatrix(SelectedBoneIndex, BoneComponentMatrix);
	const bool bShowGizmo = GetRenderOptions().ShowFlags.bGizmo && bHasBoneComponentMatrix;
	if (!bShowGizmo)
	{
		PreviewGizmo->Deactivate();
		PreviewGizmoBoneIndex = -1;
		ResetPreviewGizmoRotationDragState();
		return;
	}

	if (PreviewGizmo->IsHolding())
	{
		const FPreviewSettings& Settings = GetPreviewSettings();
		const bool bForceLocalForScale = PreviewGizmo->GetMode() == EGizmoMode::Scale;
		PreviewGizmo->SetWorldSpace(bForceLocalForScale ? false : (Settings.CoordSystem == EEditorCoordSystem::World));
		PreviewGizmo->SetSnapSettings(
			Settings.bEnableTranslationSnap, Settings.TranslationSnapSize,
			Settings.bEnableRotationSnap, Settings.RotationSnapSize,
			Settings.bEnableScaleSnap, Settings.ScaleSnapSize);
		PreviewGizmo->SetAxisMask(PreviewGizmo->ComputeAxisMaskForView(GetRenderOptions().ViewportType, Camera->GetForwardVector(), PreviewGizmo->GetMode()));
		PreviewGizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(), Camera->IsOrthogonal(), Camera->GetOrthoWidth());
		return;
	}

	const FMatrix MeshWorldMatrix = PreviewComponent->GetWorldMatrix();

	// WorldLocation = MeshWorldMatrix * BoneComponentMatrix.Location
	const FVector BoneWorldLocation = MeshWorldMatrix.TransformPositionWithW(BoneComponentMatrix.GetLocation());
	// Row-vector convention: BoneWorld = BoneComponent * MeshWorld.
	// 미러 행렬은 FQuat으로 직접 표현할 수 없으므로 회전 추출 시 내부적으로만 고정 mirror 축을 사용합니다.
	// Transform.Scale에는 음수를 노출하지 않습니다.
	const FQuat BoneWorldRotation = GetBoneWorldRotation(PreviewComponent, SelectedBoneIndex, &PreviewLastGizmoTargetRotation);

	PreviewGizmoTarget->SetWorldLocation(BoneWorldLocation);
	PreviewGizmoTarget->SetWorldRotation(BoneWorldRotation);
	PreviewLastGizmoTargetRotation = BoneWorldRotation;
	PreviewGizmoTarget->SetRelativeScale(BoneComponentMatrix.GetScale());
	PreviewGizmo->SetTarget(PreviewGizmoTarget);

	const FPreviewSettings& Settings = GetPreviewSettings();
	const bool bForceLocalForScale = PreviewGizmo->GetMode() == EGizmoMode::Scale;
	PreviewGizmo->SetWorldSpace(bForceLocalForScale ? false : (Settings.CoordSystem == EEditorCoordSystem::World));
	PreviewGizmo->SetSnapSettings(
		Settings.bEnableTranslationSnap, Settings.TranslationSnapSize,
		Settings.bEnableRotationSnap, Settings.RotationSnapSize,
		Settings.bEnableScaleSnap, Settings.ScaleSnapSize);
	PreviewGizmo->SetAxisMask(PreviewGizmo->ComputeAxisMaskForView(GetRenderOptions().ViewportType, Camera->GetForwardVector(), PreviewGizmo->GetMode()));
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
		FMatrix BoneComponentMatrix = FMatrix::Identity;
		if (!PreviewComponent->GetBoneComponentSpaceMatrix(PreviewGizmoBoneIndex, BoneComponentMatrix))
		{
			ResetPreviewGizmoRotationDragState();
			return;
		}

		FQuat CurrentGizmoRotation = GetComponentSpaceRotation(PreviewGizmoTarget, PreviewComponent);
		if (!bPreviewGizmoRotationDragInitialized || PreviewGizmoRotationDragBoneIndex != PreviewGizmoBoneIndex)
		{
			PreviewGizmoRotationDragBoneIndex = PreviewGizmoBoneIndex;
			PreviewGizmoDragStartRotation = CurrentGizmoRotation;
			PreviewBoneDragStartComponentRotation = GetBoneComponentSpaceRotation(PreviewComponent, PreviewGizmoBoneIndex);
			bPreviewGizmoRotationDragInitialized = true;
		}
		else
		{
			CurrentGizmoRotation = MakeQuatContinuous(CurrentGizmoRotation, PreviewGizmoDragStartRotation);
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

	// 이동 기즈모는 본의 ComponentSpace 위치만 갱신합니다.
	// 전체 행렬을 다시 로컬 TRS로 분해하면 부모 비균등 스케일/회전 조합에서 회전·스케일이 오염될 수 있습니다.
	PreviewComponent->SetBoneComponentSpaceLocation(PreviewGizmoBoneIndex, NewLocation);
}

void FSkeletalMeshPreviewViewportClient::ResetPreviewGizmoRotationDragState()
{
	PreviewGizmoRotationDragBoneIndex = -1;
	PreviewGizmoDragStartRotation = FQuat::Identity;
	PreviewBoneDragStartComponentRotation = FQuat::Identity;
	PreviewLastGizmoTargetRotation = FQuat::Identity;
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
		if (Snapshot.IsKeyDown(VK_MENU))
		{
			return false;
		}

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

	float Determinant3x3(const FMatrix& Matrix)
	{
		return Matrix.M[0][0] * (Matrix.M[1][1] * Matrix.M[2][2] - Matrix.M[1][2] * Matrix.M[2][1])
			- Matrix.M[0][1] * (Matrix.M[1][0] * Matrix.M[2][2] - Matrix.M[1][2] * Matrix.M[2][0])
			+ Matrix.M[0][2] * (Matrix.M[1][0] * Matrix.M[2][1] - Matrix.M[1][1] * Matrix.M[2][0]);
	}

	float QuatDot(const FQuat& A, const FQuat& B)
	{
		return A.X * B.X + A.Y * B.Y + A.Z * B.Z + A.W * B.W;
	}

	FQuat MakeQuatContinuous(const FQuat& Quat, const FQuat& ReferenceQuat)
	{
		FQuat NormalizedQuat = Quat.GetNormalized();
		const FQuat NormalizedReference = ReferenceQuat.GetNormalized();
		if (QuatDot(NormalizedQuat, NormalizedReference) < 0.0f)
		{
			NormalizedQuat.X *= -1.0f;
			NormalizedQuat.Y *= -1.0f;
			NormalizedQuat.Z *= -1.0f;
			NormalizedQuat.W *= -1.0f;
		}
		return NormalizedQuat;
	}

	float SignForScale(float Value)
	{
		return Value < 0.0f ? -1.0f : 1.0f;
	}

	FQuat ExtractRotationQuatNoScale(const FMatrix& Matrix, const FVector* PreferredScaleForMirrorAxis)
	{
		constexpr float Epsilon = 1.0e-6f;

		FVector Scale = Matrix.GetScale();
		if (PreferredScaleForMirrorAxis)
		{
			Scale.X *= SignForScale(PreferredScaleForMirrorAxis->X);
			Scale.Y *= SignForScale(PreferredScaleForMirrorAxis->Y);
			Scale.Z *= SignForScale(PreferredScaleForMirrorAxis->Z);

			const float MatrixDetSign = Determinant3x3(Matrix) < 0.0f ? -1.0f : 1.0f;
			const float ScaleDetSign = SignForScale(Scale.X) * SignForScale(Scale.Y) * SignForScale(Scale.Z);
			if (MatrixDetSign != ScaleDetSign)
			{
				Scale.X *= -1.0f;
			}
		}
		else if (Determinant3x3(Matrix) < 0.0f)
		{
			Scale.X *= -1.0f;
		}

		FMatrix RotationMatrix = FMatrix::Identity;
		const float ScaleValues[3] = { Scale.X, Scale.Y, Scale.Z };
		for (int32 Row = 0; Row < 3; ++Row)
		{
			const float RowScale = ScaleValues[Row];
			if (std::fabs(RowScale) > Epsilon)
			{
				RotationMatrix.M[Row][0] = Matrix.M[Row][0] / RowScale;
				RotationMatrix.M[Row][1] = Matrix.M[Row][1] / RowScale;
				RotationMatrix.M[Row][2] = Matrix.M[Row][2] / RowScale;
			}
		}

		FQuat Rotation = RotationMatrix.ToQuat();
		Rotation.Normalize();
		return Rotation;
	}

	// 부모-자식 계층 구조를 가진 컴포넌트의 최종 월드 공간 기준 회전값을 구합니다.
	FQuat GetStableWorldRotation(const USceneComponent* Component)
	{
		if (!Component)
		{
			return FQuat::Identity;
		}

		return ExtractRotationQuatNoScale(Component->GetWorldMatrix());
	}

	// 메쉬의 발 밑 또는 골반(Root)을 기준으로 한 회전값을 측정합니다.
	FQuat GetComponentSpaceRotation(const USceneComponent* WorldComponent, const USceneComponent* ComponentSpaceOwner)
	{
		if (!WorldComponent)
		{
			return FQuat::Identity;
		}

		if (!ComponentSpaceOwner)
		{
			return GetStableWorldRotation(WorldComponent);
		}

		return ExtractRotationQuatNoScale(WorldComponent->GetWorldMatrix() * ComponentSpaceOwner->GetWorldInverseMatrix());
	}

	FQuat GetBoneComponentSpaceRotation(const USkeletalMeshComponent* Component, int32 BoneIndex, const FQuat* ReferenceQuat)
	{
		if (!Component)
		{
			return FQuat::Identity;
		}

		FMatrix BoneComponentMatrix = FMatrix::Identity;
		if (!Component->GetBoneComponentSpaceMatrix(BoneIndex, BoneComponentMatrix))
		{
			return FQuat::Identity;
		}

		const FTransform* ComponentTransform = Component->GetBoneComponentSpaceTransform(BoneIndex);
		FQuat Rotation = ExtractRotationQuatNoScale(BoneComponentMatrix, ComponentTransform ? &ComponentTransform->Scale : nullptr);
		return ReferenceQuat ? MakeQuatContinuous(Rotation, *ReferenceQuat) : Rotation;
	}

	FQuat GetBoneWorldRotation(const USkeletalMeshComponent* Component, int32 BoneIndex, const FQuat* ReferenceQuat)
	{
		if (!Component)
		{
			return FQuat::Identity;
		}

		FMatrix BoneComponentMatrix = FMatrix::Identity;
		if (!Component->GetBoneComponentSpaceMatrix(BoneIndex, BoneComponentMatrix))
		{
			return FQuat::Identity;
		}

		const FTransform* ComponentTransform = Component->GetBoneComponentSpaceTransform(BoneIndex);
		FQuat Rotation = ExtractRotationQuatNoScale(BoneComponentMatrix * Component->GetWorldMatrix(), ComponentTransform ? &ComponentTransform->Scale : nullptr);
		return ReferenceQuat ? MakeQuatContinuous(Rotation, *ReferenceQuat) : Rotation;
	}

	// Matrix에서 FTransform 정보를 추출할 때도 미러/음수 determinant 회전 추출 규칙을 동일하게 사용합니다.
	FTransform TransformFromMatrix(const FMatrix& Matrix)
	{
		return FTransform(Matrix.GetLocation(), ExtractRotationQuatNoScale(Matrix), Matrix.GetScale());
	}

	// int32 PreviewMode 값을 EGizmoMode Enum 값으로 변환합니다. 
	EGizmoMode GizmoModeFromPreviewMode(int32 PreviewMode)
	{
		switch (PreviewMode)
		{
		case 1: return EGizmoMode::Rotate;
		case 2: return EGizmoMode::Scale;
		default: return EGizmoMode::Translate;
		}
	}

	// EGizmoMode Enum 값을 int32 PreviewMode 값으로 변환합니다.
	int32 PreviewModeFromGizmoMode(EGizmoMode Mode)
	{
		switch (Mode)
		{
		case EGizmoMode::Rotate: return 1;
		case EGizmoMode::Scale: return 2;
		default: return 0;
		}
	}
}
