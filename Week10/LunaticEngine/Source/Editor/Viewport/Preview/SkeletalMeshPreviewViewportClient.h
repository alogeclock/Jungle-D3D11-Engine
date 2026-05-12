#pragma once

#include "PreviewViewportClient.h"
#include "Math/Quat.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UGizmoComponent;
class AActor;
class USceneComponent;
struct FInputSystemSnapshot;

// 특정 스켈레탈 메시를 독립된 프리뷰 환경(Preview Scene)에 렌더링하고 제어합니다.
// 메시 교체, 카메라 포커스 맞춤, 본 구조 시각화 등 스켈레탈 메시 에디터의 핵심 뷰포트 로직을 담당합니다.
class	FSkeletalMeshPreviewViewportClient : public FPreviewViewportClient
{
public:
	FSkeletalMeshPreviewViewportClient();
	~FSkeletalMeshPreviewViewportClient() override;
	
	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
	USkeletalMesh* GetSkeletalMesh() const { return PreviewSkeletalMesh; }
	USkeletalMeshComponent* GetPreviewComponent() const { return PreviewComponent; }
	int32 PickBoneAtViewportPosition(float LocalX, float LocalY, float ViewportWidth, float ViewportHeight) const;
	bool IsPreviewGizmoHitAtViewportPosition(float LocalX, float LocalY, float ViewportWidth, float ViewportHeight) const;
	UGizmoComponent* GetPreviewGizmo() const { return PreviewGizmo; }
	void SetPreviewGizmoMode(int32 InMode) override;
	int32 GetPreviewGizmoMode() const override;

	bool FocusPreviewMesh();
	void Tick(float DeltaTime) override;
	bool HandleInputSnapshot(const FInputSystemSnapshot& Snapshot, float DeltaTime) override;

protected:
	void OnCameraReset() override;

private:
	void CreatePreviewComponent();
	void DestroyPreviewComponent();
	void CreatePreviewGizmo();
	void DestroyPreviewGizmo();
	void SyncPreviewGizmoToSelectedBone();
	void ApplyPreviewGizmoToSelectedBone();
	void ResetPreviewGizmoRotationDragState();
	bool HandlePreviewGizmoInput(const FInputSystemSnapshot& Snapshot, float DeltaTime);
	void UpdateOrbitTargetForAltNavigation();

private:
	USkeletalMesh* PreviewSkeletalMesh = nullptr;
	AActor* PreviewActor = nullptr;
	USkeletalMeshComponent* PreviewComponent = nullptr;
	USceneComponent* PreviewGizmoTarget = nullptr;
	UGizmoComponent* PreviewGizmo = nullptr;
	int32 PreviewGizmoBoneIndex = -1;
	int32 PreviewGizmoRotationDragBoneIndex = -1;
	FQuat PreviewGizmoDragStartRotation = FQuat::Identity;
	FQuat PreviewBoneDragStartComponentRotation = FQuat::Identity;
	FVector PreviewBoneDragStartLocalScale = FVector(1.0f, 1.0f, 1.0f);
	bool bPreviewGizmoRotationDragInitialized = false;
};
