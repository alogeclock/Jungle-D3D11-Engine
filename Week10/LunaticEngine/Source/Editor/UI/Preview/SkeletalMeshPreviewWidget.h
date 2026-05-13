#pragma once

#include "AssetPreviewWidget.h"
#include "Core/CoreTypes.h"
#include "Editor/UI/EditorDetailsWidget.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"

class FWindowsWindow;
class FSkeletalMeshPreviewViewportClient;
class UEditorEngine;
class USkeletalMesh;
class UTexture2D;
struct FSkeleton;

enum class EBoneRotationEditSpace : uint8
{
	Local,
	Component
};

struct FBoneRotationEditState
{
	int32 BoneIndex = -1;
	FQuat Quat = FQuat::Identity;
	FRotator EulerHint = FRotator();
};
// 스켈레탈 메시 애셋을 미리보고 본 구조 및 데이터를 수정하는 위젯 클래스입니다.
class FSkeletalMeshPreviewWidget : public FAssetPreviewWidget
{
public:
	// Main Functions
    void Initialize(UEditorEngine* InEngine, ID3D11Device* InDevice, FWindowsWindow* InWindow);
    void Shutdown() override;

	void Render(float DeltaTime) override;

	void OpenSkeletalMesh(USkeletalMesh* InMesh);
    void SetEditorInstanceId(int32 InInstanceId) { EditorInstanceId = InInstanceId; }

protected:
	std::unique_ptr<FPreviewViewportClient> CreatePreviewViewportClient() override;

private:
	FSkeletalMeshPreviewViewportClient* GetSkeletalViewportClient() const;

    // UI Rendering
	void RebuildBoneWeightedFlags();
    void DrawBoneHierarchyPanel();
    void DrawBoneDetailsPanel();
    void DrawMorphTargetPanel();
    void DrawPreviewComponentDetailsPanel();
    void DrawBoneTreeNode(const FSkeleton& Skeleton, int32 BoneIndex);
    bool DrawEditableTransform(const char* SectionName, FTransform& Transform, int32 BoneIndex, FBoneRotationEditState& State, EBoneRotationEditSpace Space);
	void EnsureBoneTreeIcons();

    // Interaction & Validation
    void HandleViewportBoneSelection();
    void ValidateSelectedBone();

private:
	// Widget-Specific Clients & Widget
    FEditorDetailsWidget ComponentDetailsWidget;

	// Skeletal Mesh Data
	USkeletalMesh* EditingMesh = nullptr;
	int32 SelectedBoneIndex = -1;
	TArray<bool> BoneWeightedFlags;

    // UI Icons
    UTexture2D* WeightedBoneIcon = nullptr;
    UTexture2D* NonWeightedBoneIcon = nullptr;

    // Bone Rotation State
    FBoneRotationEditState LocalRotationEditState;
    FBoneRotationEditState ComponentRotationEditState;

    bool bInitialized = false;
};
