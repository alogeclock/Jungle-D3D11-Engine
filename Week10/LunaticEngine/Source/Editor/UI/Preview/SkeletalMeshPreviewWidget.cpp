#include "SkeletalMeshPreviewWidget.h"

#include "Component/SkeletalMeshComponent.h"
#include "Core/Notification.h"
#include "Editor/EditorEngine.h"
#include "Math/Rotator.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <filesystem>

namespace
{
	FSkeletalMesh* GetSkeletalMeshAsset(USkeletalMesh* Mesh)
	{
		return Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	}

	const FSkeleton* GetSkeleton(USkeletalMesh* Mesh)
	{
		FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(Mesh);
		return MeshAsset ? &MeshAsset->Skeleton : nullptr;
	}

	bool IsValidBoneIndex(const FSkeleton* Skeleton, int32 BoneIndex)
	{
		return Skeleton && BoneIndex >= 0 && BoneIndex < static_cast<int32>(Skeleton->Bones.size());
	}

	FString GetMeshDisplayName(USkeletalMesh* Mesh)
	{
		if (!Mesh)
		{
			return "Skeletal Mesh Editor";
		}

		const FString& AssetPath = Mesh->GetAssetPathFileName();
		if (AssetPath.empty())
		{
			return "Skeletal Mesh Editor";
		}

		return std::filesystem::path(AssetPath).filename().string();
	}

	USkeletalMeshComponent* GetPreviewComponent(FSkeletalMeshPreviewViewportClient& ViewportClient)
	{
		return ViewportClient.GetPreviewComponent();
	}

	void SelectBone(FSkeletalMeshPreviewViewportClient& ViewportClient, int32& SelectedBoneIndex, int32 BoneIndex)
	{
		SelectedBoneIndex = BoneIndex;
		if (USkeletalMeshComponent* PreviewComponent = GetPreviewComponent(ViewportClient))
		{
			PreviewComponent->SetSelectedBoneIndex(BoneIndex);
			SelectedBoneIndex = PreviewComponent->GetSelectedBoneIndex();
		}
	}

	bool DrawEditableTransform(const char* Label, FTransform& Transform)
	{
		bool bChanged = false;
		ImGui::TextUnformatted(Label);
		ImGui::Indent();

		float Location[3] = { Transform.Location.X, Transform.Location.Y, Transform.Location.Z };
		if (ImGui::DragFloat3("Location", Location, 0.05f))
		{
			Transform.Location = FVector(Location[0], Location[1], Location[2]);
			bChanged = true;
		}

		FRotator Rotator = Transform.GetRotator();
		float Rotation[3] = { Rotator.Roll, Rotator.Pitch, Rotator.Yaw };
		if (ImGui::DragFloat3("Rotation", Rotation, 0.1f))
		{
			Transform.SetRotation(FRotator(Rotation[1], Rotation[2], Rotation[0]));
			bChanged = true;
		}

		float Scale[3] = { Transform.Scale.X, Transform.Scale.Y, Transform.Scale.Z };
		if (ImGui::DragFloat3("Scale", Scale, 0.01f, 0.001f, 1000.0f))
		{
			Transform.Scale = FVector(Scale[0], Scale[1], Scale[2]);
			bChanged = true;
		}

		ImGui::Unindent();
		return bChanged;
	}

	void DrawTransformMatrixRows(const char* Label, const FMatrix& Matrix)
	{
		const FVector Location = Matrix.GetLocation();
		const FVector Rotation = Matrix.GetEuler();
		const FVector Scale = Matrix.GetScale();

		ImGui::TextUnformatted(Label);
		ImGui::Indent();
		ImGui::Text("Location  %.3f, %.3f, %.3f", Location.X, Location.Y, Location.Z);
		ImGui::Text("Rotation  %.3f, %.3f, %.3f", Rotation.X, Rotation.Y, Rotation.Z);
		ImGui::Text("Scale     %.3f, %.3f, %.3f", Scale.X, Scale.Y, Scale.Z);
		ImGui::Unindent();
	}
}

void FSkeletalMeshPreviewWidget::Initialize(UEditorEngine* InEngine, ID3D11Device* InDevice, FWindowsWindow* InWindow)
{
	if (bInitialized)
	{
		return;
	}

	Engine = InEngine;

	Viewport = new FViewport();
	if (!Viewport->Initialize(InDevice, 512, 512))
	{
		delete Viewport;
		Viewport = nullptr;
		FNotificationManager::Get().AddNotification("Failed to initialize SkeletalMesh preview viewport.", ENotificationType::Error, 5.0f);
		return;
	}

	Viewport->SetClient(&ViewportClient);
	ViewportClient.SetViewport(Viewport);
	ViewportClient.Initialize(InWindow);
	ViewportWidget.SetViewportClient(&ViewportClient);
	bInitialized = true;
}

void FSkeletalMeshPreviewWidget::Shutdown()
{
	Close();

	ViewportClient.SetViewport(nullptr);
	ViewportWidget.SetViewportClient(nullptr);

	if (Viewport)
	{
		Viewport->Release();
		delete Viewport;
		Viewport = nullptr;
	}

	Engine = nullptr;
	bInitialized = false;
	bCapturingInput = false;
}

void FSkeletalMeshPreviewWidget::OpenSkeletalMesh(USkeletalMesh* InMesh)
{
	if (!bInitialized || !InMesh)
	{
		return;
	}

	EditingMesh = InMesh;
	SelectedBoneIndex = -1;
	ViewportClient.SetSkeletalMesh(EditingMesh);
	bOpen = true;
	bCapturingInput = false;
	RegisterPreviewClient();
}

void FSkeletalMeshPreviewWidget::Render(float DeltaTime)
{
	if (!bOpen)
	{
		bCapturingInput = false;
		return;
	}

	bool bWindowOpen = bOpen;
	ImGui::SetNextWindowSize(ImVec2(720.0f, 560.0f), ImGuiCond_FirstUseEver);
	const FString WindowTitle = "Skeletal Mesh Editor - " + GetMeshDisplayName(EditingMesh) + "###SkeletalMeshEditor" + std::to_string(EditorInstanceId);
	const bool bVisible = ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, ImGuiWindowFlags_NoCollapse);
	if (!bWindowOpen)
	{
		ImGui::End();
		Close();
		return;
	}

	if (!bVisible)
	{
		ImGui::End();
		ViewportClient.SetHovered(false);
		ViewportClient.SetActive(false);
		bCapturingInput = false;
		return;
	}

	ValidateSelectedBone();

	const ImVec2 ContentRegion = ImGui::GetContentRegionAvail();
	if (ImGui::BeginTable("SkeletalMeshEditorLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, ContentRegion.y)))
	{
		ImGui::TableSetupColumn("Skeleton", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Viewport", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 260.0f);

		ImGui::TableNextColumn();
		DrawBoneHierarchyPanel();

		ImGui::TableNextColumn();
		ViewportWidget.Render(DeltaTime);
		HandleViewportBoneSelection();

		ImGui::TableNextColumn();
		DrawBoneDetailsPanel();

		ImGui::EndTable();
	}
	else
	{
		ViewportWidget.Render(DeltaTime);
	}

	bCapturingInput = ViewportClient.IsHovered() || ViewportClient.IsActive();

	ImGui::End();
}

void FSkeletalMeshPreviewWidget::ClearInputCapture()
{
	ViewportClient.SetHovered(false);
	ViewportClient.SetActive(false);
	bCapturingInput = false;
}

void FSkeletalMeshPreviewWidget::Close()
{
	if (!bOpen && !bRegistered && !EditingMesh)
	{
		return;
	}

	UnregisterPreviewClient();
	ClearInputCapture();
	ViewportClient.SetSkeletalMesh(nullptr);
	EditingMesh = nullptr;
	SelectedBoneIndex = -1;
	bOpen = false;
	bCapturingInput = false;
}

void FSkeletalMeshPreviewWidget::RegisterPreviewClient()
{
	if (!Engine || bRegistered)
	{
		return;
	}

	Engine->RegisterPreviewViewportClient(&ViewportClient);
	bRegistered = true;
}

void FSkeletalMeshPreviewWidget::UnregisterPreviewClient()
{
	if (!Engine || !bRegistered)
	{
		return;
	}

	Engine->UnregisterPreviewViewportClient(&ViewportClient);
	bRegistered = false;
}

void FSkeletalMeshPreviewWidget::DrawBoneHierarchyPanel()
{
	ImGui::TextUnformatted("Bone Hierarchy");
	ImGui::Separator();

	const FSkeleton* Skeleton = GetSkeleton(EditingMesh);
	if (!Skeleton)
	{
		ImGui::TextDisabled("No skeletal mesh.");
		return;
	}

	const int32 BoneCount = static_cast<int32>(Skeleton->Bones.size());
	if (BoneCount == 0)
	{
		ImGui::TextDisabled("No bones.");
		return;
	}

	if (ImGui::Button("Clear Selection"))
	{
		SelectBone(ViewportClient, SelectedBoneIndex, -1);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%d bones", BoneCount);

	ImGui::BeginChild("##BoneHierarchyTree", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const FBoneInfo& Bone = Skeleton->Bones[BoneIndex];
		const bool bHasValidParent = Bone.ParentIndex >= 0 && Bone.ParentIndex < BoneCount;
		if (!bHasValidParent)
		{
			DrawBoneTreeNode(*Skeleton, BoneIndex);
		}
	}
	ImGui::EndChild();
}

void FSkeletalMeshPreviewWidget::DrawBoneDetailsPanel()
{
	ImGui::TextUnformatted("Details");
	ImGui::Separator();

	ImGui::BeginChild("##BoneDetailsScroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

	const FSkeleton* Skeleton = GetSkeleton(EditingMesh);
	if (!IsValidBoneIndex(Skeleton, SelectedBoneIndex))
	{
		ImGui::TextDisabled("Select a bone from the hierarchy.");
		ImGui::EndChild();
		return;
	}

	const FBoneInfo& Bone = Skeleton->Bones[SelectedBoneIndex];
	USkeletalMeshComponent* PreviewComponent = GetPreviewComponent(ViewportClient);
	ImGui::Text("Name: %s", Bone.Name.c_str());
	ImGui::Text("Index: %d", SelectedBoneIndex);
	if (IsValidBoneIndex(Skeleton, Bone.ParentIndex))
	{
		const FBoneInfo& ParentBone = Skeleton->Bones[Bone.ParentIndex];
		ImGui::Text("Parent: %s (%d)", ParentBone.Name.c_str(), Bone.ParentIndex);
	}
	else
	{
		ImGui::TextUnformatted("Parent: None");
	}
	ImGui::Text("Children: %d", static_cast<int32>(Bone.Children.size()));

	if (PreviewComponent)
	{
		ImGui::Spacing();
		ImGui::TextUnformatted("Current Pose");
		ImGui::Separator();

		if (const FTransform* LocalTransform = PreviewComponent->GetBoneLocalTransform(SelectedBoneIndex))
		{
			FTransform EditableLocal = *LocalTransform;
			ImGui::PushID("LocalTransform");
			if (DrawEditableTransform("Local", EditableLocal))
			{
				PreviewComponent->SetBoneLocalTransform(SelectedBoneIndex, EditableLocal);
			}
			ImGui::PopID();
		}

		if (const FTransform* ComponentTransform = PreviewComponent->GetBoneComponentSpaceTransform(SelectedBoneIndex))
		{
			FTransform EditableComponent = *ComponentTransform;
			ImGui::PushID("ComponentTransform");
			if (DrawEditableTransform("Component", EditableComponent))
			{
				PreviewComponent->SetBoneComponentSpaceTransform(SelectedBoneIndex, EditableComponent);
			}
			ImGui::PopID();
		}
	}

	ImGui::Spacing();
	ImGui::TextUnformatted("Bind Pose");
	ImGui::Separator();
	DrawTransformMatrixRows("Local", Bone.LocalBindPose);
	ImGui::Spacing();
	DrawTransformMatrixRows("Global", Bone.GlobalBindPose);

	ImGui::EndChild();
}

void FSkeletalMeshPreviewWidget::DrawBoneTreeNode(const FSkeleton& Skeleton, int32 BoneIndex)
{
	if (!IsValidBoneIndex(&Skeleton, BoneIndex))
	{
		return;
	}

	const FBoneInfo& Bone = Skeleton.Bones[BoneIndex];
	const bool bSelected = BoneIndex == SelectedBoneIndex;
	const bool bLeaf = Bone.Children.empty();
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (bLeaf)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (bSelected)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	ImGui::PushID(BoneIndex);
	const FString Label = Bone.Name.empty() ? FString("<Unnamed Bone>") : Bone.Name;
	const bool bOpenNode = ImGui::TreeNodeEx(Label.c_str(), Flags);
	if (ImGui::IsItemClicked())
	{
		SelectBone(ViewportClient, SelectedBoneIndex, BoneIndex);
	}

	if (bOpenNode && !bLeaf)
	{
		for (int32 ChildIndex : Bone.Children)
		{
			DrawBoneTreeNode(Skeleton, ChildIndex);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void FSkeletalMeshPreviewWidget::HandleViewportBoneSelection()
{
	if (!ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		return;
	}

	const ImVec2 ItemMin = ImGui::GetItemRectMin();
	const ImVec2 ItemMax = ImGui::GetItemRectMax();
	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	const float Width = ItemMax.x - ItemMin.x;
	const float Height = ItemMax.y - ItemMin.y;
	const float LocalX = MousePos.x - ItemMin.x;
	const float LocalY = MousePos.y - ItemMin.y;

	if (ViewportClient.IsPreviewGizmoHitAtViewportPosition(LocalX, LocalY, Width, Height))
	{
		return;
	}

	const int32 PickedBoneIndex = ViewportClient.PickBoneAtViewportPosition(LocalX, LocalY, Width, Height);
	SelectBone(ViewportClient, SelectedBoneIndex, PickedBoneIndex);
}

void FSkeletalMeshPreviewWidget::ValidateSelectedBone()
{
	const FSkeleton* Skeleton = GetSkeleton(EditingMesh);
	if (USkeletalMeshComponent* PreviewComponent = GetPreviewComponent(ViewportClient))
	{
		const int32 ComponentSelectedBoneIndex = PreviewComponent->GetSelectedBoneIndex();
		if (ComponentSelectedBoneIndex != SelectedBoneIndex)
		{
			SelectedBoneIndex = ComponentSelectedBoneIndex;
		}
	}

	if (!IsValidBoneIndex(Skeleton, SelectedBoneIndex))
	{
		SelectedBoneIndex = -1;
		if (USkeletalMeshComponent* PreviewComponent = GetPreviewComponent(ViewportClient))
		{
			PreviewComponent->SetSelectedBoneIndex(-1);
		}
	}
}
