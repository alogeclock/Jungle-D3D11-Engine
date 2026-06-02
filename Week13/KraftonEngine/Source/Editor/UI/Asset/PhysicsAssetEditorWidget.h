#pragma once

#include "AssetEditorWidget.h"
#include "Editor/Slate/SWindow.h"
#include "Editor/Subsystem/AssetFactory.h"
#include "Editor/Viewport/MeshEditorViewportClient.h"

struct ID3D11ShaderResourceView;

class AActor;
class IGizmoTransformTarget;
class UMaterial;
class UPhysicsAsset;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;

struct FSkeletonAsset;
enum class EPhysicsAssetPreviewShapeType : uint8
{
	None,
	Sphere,
	Box,
	Capsule,
	Convex
};

class FPhysicsAssetEditorWidget : public FAssetEditorWidget
{
	enum class EPhysicsAssetConstraintVisualType : uint8
	{
		SwingCone,
		TwistFan
	};

	struct FPreviewConstraintConeEntry
	{
		UStaticMeshComponent* Component = nullptr;
		UMaterial* Material = nullptr;
		UStaticMesh* Mesh = nullptr;
		int32 ConstraintIndex = -1;
		EPhysicsAssetConstraintVisualType VisualType = EPhysicsAssetConstraintVisualType::SwingCone;
		float EffectivePrimaryLimit = 0.0f;
		float EffectiveSecondaryLimit = 0.0f;
	};

	struct FPreviewShapeComponentEntry
	{
		UStaticMeshComponent* Component = nullptr;
		UMaterial* Material = nullptr;
		int32 BodyIndex = -1;
		int32 ShapeIndex = -1;
		int32 PartIndex = 0;
		EPhysicsAssetPreviewShapeType ShapeType = EPhysicsAssetPreviewShapeType::None;
		FVector BaseMeshExtent = FVector::OneVector;
	};

public:
	FPhysicsAssetEditorWidget();

	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;
	void Render(float DeltaTime) override;
	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

private:
	void InitializePreviewScene(UPhysicsAsset* PhysicsAsset);
	void ClearPreviewShapeComponents();
	void RebuildPreviewShapeComponents(UPhysicsAsset* PhysicsAsset);
	void SyncPreviewShapeComponents(UPhysicsAsset* PhysicsAsset);
	void ClearConstraintConeComponents();
	bool NeedsConstraintConeRebuild(UPhysicsAsset* PhysicsAsset) const;
	void RebuildConstraintConeComponents(UPhysicsAsset* PhysicsAsset);
	void SyncConstraintConeComponents(UPhysicsAsset* PhysicsAsset);
	void DrawConstraintDebug(UPhysicsAsset* PhysicsAsset);
	UStaticMeshComponent* CreatePreviewShapeComponent(UStaticMesh* StaticMesh, int32 BodyIndex, EPhysicsAssetPreviewShapeType ShapeType, int32 ShapeIndex, int32 PartIndex, const FVector& BaseMeshExtent);
	UMaterial* CreatePreviewShapeMaterial(float Alpha, bool bDisableDepthTest = false) const;
	void SelectBodyShape(int32 BodyIndex, EPhysicsAssetPreviewShapeType ShapeType, int32 ShapeIndex);
	void SelectBoneByIndex(int32 BoneIndex);
	void SelectBodyByIndex(int32 BodyIndex);
	void SelectConstraintByIndex(int32 ConstraintIndex);
	void ClearSelection();
	void RenderSkeletonTree(const FSkeletonAsset* SkeletonAsset, int32 BoneIndex, const FString& SearchText);
	bool RenderGraphPanel(UPhysicsAsset* PhysicsAsset);
	bool RenderDetailsPanel(UPhysicsAsset* PhysicsAsset);
	bool RenderPreviewSettingsPanel(bool bShowHeader = true);
	bool RenderToolsPanel(UPhysicsAsset* PhysicsAsset);
	void SyncSelectionToViewport();
	int32 FindBoneIndexByName(const FSkeletonAsset* SkeletonAsset, const FName& BoneName) const;

private:
	uint32 InstanceId = 0;
	FName PreviewWorldHandle;
	FString WindowIdSuffix;
	SWindow PhysicsAssetViewportWindow;
	FMeshEditorViewportClient ViewportClient;
	USkeletalMesh* PreviewSkeletalMesh = nullptr;
	AActor* PreviewShapeActor = nullptr;
	UStaticMesh* PreviewCubeMesh = nullptr;
	UStaticMesh* PreviewSphereMesh = nullptr;
	UStaticMesh* PreviewHemisphereMesh = nullptr;
	UStaticMesh* PreviewCylinderMesh = nullptr;
	UStaticMesh* PreviewOpenCylinderMesh = nullptr;
	UStaticMesh* PreviewCapsuleMesh = nullptr;

	FPhysicsAssetCreateParams ToolSettings;
	TArray<FPreviewShapeComponentEntry> PreviewShapeComponents;
	TArray<FPreviewConstraintConeEntry> PreviewConstraintCones;
	IGizmoTransformTarget* BodyGizmoTarget = nullptr;
	IGizmoTransformTarget* ConstraintGizmoTarget = nullptr;

	int32 SelectedBoneIndex = -1;
	int32 SelectedBodyIndex = -1;
	int32 SelectedConstraintIndex = -1;
	EPhysicsAssetPreviewShapeType SelectedShapeType = EPhysicsAssetPreviewShapeType::None;
	int32 SelectedShapeIndex = -1;
	FVector PreviewShapeTint = FVector(1.0f, 1.0f, 1.0f);
	float PreviewBaseShapeOpacity = 0.3f;
	float PreviewConstraintShapeOpacity = 0.3f;
	bool bShowConstraintDebug = true;
	float ConstraintAxisLength = 0.1f;

	char BoneSearchBuffer[128] = {};
	bool bShowBonesWithBodiesOnly = false;
	bool bExpandBodyTreeOnNextRender = false;
	bool bFrameGraphOnNextRender = false;
	bool bScrollToSelectedConstraintOnNextRender = false;
	bool bPendingClose = false;
	bool bConstraintPreviewDefinitionDirty = false;
	bool bConstraintPreviewTransformDirty = false;

	ID3D11ShaderResourceView* CapsuleBodyIcon = nullptr;
	int32 BoneTreeRowCounter = 0;
};
