#include "Editor/UI/EditorSkeletalMeshViewer.h"

#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/UI/EditorPanelTitleUtils.h"
#include "Platform/Paths.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
	FString ToLowerAscii(FString Value)
	{
		for (char& Character : Value)
		{
			Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
		}
		return Value;
	}

	FString MakeDisplayName(const std::filesystem::path& Path)
	{
		return FPaths::ToUtf8(Path.filename().wstring());
	}

	bool IsPreviewSkeletalMeshFile(const std::filesystem::path& Path)
	{
		const FString Extension = ToLowerAscii(FPaths::ToUtf8(Path.extension().wstring()));
		return Extension == ".skel"
			|| Extension == ".skmesh"
			|| Extension == ".skeletalmesh"
			|| Extension == ".fbx"
			|| Extension == ".bin";
	}

	void ScanDirectoryForAssets(const std::filesystem::path& Root, TArray<FEditorSkeletalMeshViewer::FSkeletalMeshAssetItem>& OutItems)
	{
		if (!std::filesystem::exists(Root) || !std::filesystem::is_directory(Root))
		{
			return;
		}

		std::error_code Error;
		for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(Root, Error))
		{
			if (Error)
			{
				break;
			}

			if (!Entry.is_regular_file() || !IsPreviewSkeletalMeshFile(Entry.path()))
			{
				continue;
			}

			FEditorSkeletalMeshViewer::FSkeletalMeshAssetItem Item;
			Item.DisplayName = MakeDisplayName(Entry.path());
			Item.Path = Entry.path().lexically_normal();
			OutItems.push_back(Item);
		}
	}

	ImVec2 ProjectBonePosition(const FVector& Position, const ImVec2& Origin, const ImVec2& Size)
	{
		const float Scale = (std::min)(Size.x, Size.y) * 0.18f;
		const float X = Origin.x + Size.x * 0.5f + Position.Y * Scale;
		const float Y = Origin.y + Size.y * 0.78f - Position.Z * Scale;
		return ImVec2(X, Y);
	}

	ImU32 BoneColor(bool bSelected)
	{
		return bSelected ? EditorAccentColor::ToU32() : IM_COL32(210, 214, 222, 255);
	}
}

void FEditorSkeletalMeshViewer::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	RefreshAssetList();
	BuildFallbackSkeleton("Preview");
}

void FEditorSkeletalMeshViewer::Render(float DeltaTime)
{
	(void)DeltaTime;

	FEditorSettings& Settings = FEditorSettings::Get();
	if (!Settings.UI.bSkeletalMeshViewer)
	{
		return;
	}

	if (bAutoRefreshRequested)
	{
		RefreshAssetList();
		bAutoRefreshRequested = false;
	}

	constexpr const char* PanelIconKey = "Editor.Icon.ContentBrowser.Mesh";
	const std::string WindowTitle = EditorPanelTitleUtils::MakeClosablePanelTitle("Skeletal Mesh Viewer", PanelIconKey);
	const bool bIsOpen = ImGui::Begin(WindowTitle.c_str());
	EditorPanelTitleUtils::DrawPanelTitleIcon(PanelIconKey);
	EditorPanelTitleUtils::DrawSmallPanelCloseButton("    Skeletal Mesh Viewer", Settings.UI.bSkeletalMeshViewer, "x##CloseSkeletalMeshViewer");
	if (!bIsOpen)
	{
		ImGui::End();
		return;
	}
	EditorPanelTitleUtils::ApplyPanelContentTopInset();

	RenderToolbar();
	ImGui::Separator();

	if (ImGui::BeginTable("SkeletalMeshViewerLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthFixed, 240.0f);
		ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 320.0f);

		ImGui::TableNextColumn();
		RenderAssetList();

		ImGui::TableNextColumn();
		RenderPreview();

		ImGui::TableNextColumn();
		RenderBoneHierarchy();
		ImGui::Separator();
		RenderSelectedBoneDetails();

		ImGui::EndTable();
	}

	ImGui::End();
}

// Skeletal Mesh Asset을 Asset/ 폴더, Data/ 폴더에서 불러온다.
void FEditorSkeletalMeshViewer::RefreshAssetList()
{
	AssetItems.clear();

	const std::filesystem::path RootDir = std::filesystem::path(FPaths::RootDir()).lexically_normal();
	ScanDirectoryForAssets(RootDir / L"Asset", AssetItems);
	ScanDirectoryForAssets(RootDir / L"Data", AssetItems);

	std::sort(AssetItems.begin(), AssetItems.end(),
		[](const FSkeletalMeshAssetItem& A, const FSkeletalMeshAssetItem& B)
		{
			return A.DisplayName < B.DisplayName;
		});

	if (SelectedAssetIndex >= static_cast<int32>(AssetItems.size()))
	{
		SelectedAssetIndex = -1;
		OpenAssetPath.clear();
	}
}

// Skeletal Mesh Asset을 불러온다.
void FEditorSkeletalMeshViewer::OpenAsset(int32 AssetIndex)
{
	if (AssetIndex < 0 || AssetIndex >= static_cast<int32>(AssetItems.size()))
	{
		return;
	}

	SelectedAssetIndex = AssetIndex;
	OpenAssetPath = AssetItems[AssetIndex].Path;
	BuildFallbackSkeleton(AssetItems[AssetIndex].DisplayName);
}

// 입력된 Skeletal Mesh Asset이 없을 시 기본 스켈레톤을 표시한다. (디버그 전용)
void FEditorSkeletalMeshViewer::BuildFallbackSkeleton(const FString& AssetName)
{
	PreviewBones.clear();

	FPreviewBone Root;
	Root.Name = AssetName.empty() ? "Root" : AssetName;
	Root.ParentIndex = -1;
	Root.LocalTranslation = FVector(0.0f, 0.0f, 0.0f);
	PreviewBones.push_back(Root);

	FPreviewBone Pelvis;
	Pelvis.Name = "pelvis";
	Pelvis.ParentIndex = 0;
	Pelvis.LocalTranslation = FVector(0.0f, 0.0f, 0.8f);
	PreviewBones.push_back(Pelvis);

	FPreviewBone Spine;
	Spine.Name = "spine_01";
	Spine.ParentIndex = 1;
	Spine.LocalTranslation = FVector(0.0f, 0.0f, 0.75f);
	PreviewBones.push_back(Spine);

	FPreviewBone Head;
	Head.Name = "head";
	Head.ParentIndex = 2;
	Head.LocalTranslation = FVector(0.0f, 0.0f, 0.65f);
	PreviewBones.push_back(Head);

	FPreviewBone LeftArm;
	LeftArm.Name = "upperarm_l";
	LeftArm.ParentIndex = 2;
	LeftArm.LocalTranslation = FVector(0.0f, -0.75f, 0.35f);
	PreviewBones.push_back(LeftArm);

	FPreviewBone RightArm;
	RightArm.Name = "upperarm_r";
	RightArm.ParentIndex = 2;
	RightArm.LocalTranslation = FVector(0.0f, 0.75f, 0.35f);
	PreviewBones.push_back(RightArm);

	FPreviewBone LeftLeg;
	LeftLeg.Name = "thigh_l";
	LeftLeg.ParentIndex = 1;
	LeftLeg.LocalTranslation = FVector(0.0f, -0.35f, -0.8f);
	PreviewBones.push_back(LeftLeg);

	FPreviewBone RightLeg;
	RightLeg.Name = "thigh_r";
	RightLeg.ParentIndex = 1;
	RightLeg.LocalTranslation = FVector(0.0f, 0.35f, -0.8f);
	PreviewBones.push_back(RightLeg);

	SelectedBoneIndex = PreviewBones.empty() ? -1 : 0;
}

void FEditorSkeletalMeshViewer::RenderToolbar()
{
	if (ImGui::Button("Refresh##SkeletalMeshViewerRefresh"))
	{
		RefreshAssetList();
	}
	ImGui::SameLine();

	if (SelectedAssetIndex >= 0 && SelectedAssetIndex < static_cast<int32>(AssetItems.size()))
	{
		ImGui::TextDisabled("%s", AssetItems[SelectedAssetIndex].DisplayName.c_str());
	}
	else
	{
		ImGui::TextDisabled("No skeletal mesh selected");
	}
}

void FEditorSkeletalMeshViewer::RenderAssetList()
{
	ImGui::BeginChild("SkeletalMeshAssetList", ImVec2(0.0f, 0.0f), true);
	ImGui::TextUnformatted("Resources");
	ImGui::Separator();

	if (AssetItems.empty())
	{
		ImGui::TextDisabled("No preview assets found.");
		ImGui::TextDisabled("Scans Asset/ and Data/.");
		ImGui::EndChild();
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(AssetItems.size()); ++Index)
	{
		const bool bSelected = Index == SelectedAssetIndex;
		if (ImGui::Selectable(AssetItems[Index].DisplayName.c_str(), bSelected))
		{
			OpenAsset(Index);
		}

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(FPaths::ToUtf8(AssetItems[Index].Path.wstring()).c_str());
			ImGui::EndTooltip();
		}
	}

	ImGui::EndChild();
}

void FEditorSkeletalMeshViewer::RenderPreview()
{
	ImGui::BeginChild("SkeletalMeshPreview", ImVec2(0.0f, 0.0f), true);

	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 Size = ImGui::GetContentRegionAvail();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(
		Origin,
		ImVec2(Origin.x + Size.x, Origin.y + Size.y),
		IM_COL32(14, 16, 18, 255));
	DrawList->AddRect(
		Origin,
		ImVec2(Origin.x + Size.x, Origin.y + Size.y),
		IM_COL32(68, 72, 80, 255));

	RenderSkeletonOverlay(Origin, Size);
	ImGui::InvisibleButton("##SkeletalMeshPreviewCanvas", Size);

	ImGui::EndChild();
}

void FEditorSkeletalMeshViewer::RenderBoneHierarchy()
{
	ImGui::BeginChild("SkeletalMeshBoneHierarchy", ImVec2(0.0f, ImGui::GetContentRegionAvail().y * 0.52f), true);
	ImGui::TextUnformatted("Bone Hierarchy");
	ImGui::Separator();

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(PreviewBones.size()); ++BoneIndex)
	{
		if (PreviewBones[BoneIndex].ParentIndex == -1)
		{
			RenderBoneNode(BoneIndex);
		}
	}

	ImGui::EndChild();
}

void FEditorSkeletalMeshViewer::RenderBoneNode(int32 BoneIndex)
{
	if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(PreviewBones.size()))
	{
		return;
	}

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (SelectedBoneIndex == BoneIndex)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	bool bHasChildren = false;
	for (const FPreviewBone& Bone : PreviewBones)
	{
		if (Bone.ParentIndex == BoneIndex)
		{
			bHasChildren = true;
			break;
		}
	}
	if (!bHasChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf;
	}

	ImGui::PushID(BoneIndex);
	const bool bOpen = ImGui::TreeNodeEx("##BoneNode", Flags, "%s", PreviewBones[BoneIndex].Name.c_str());
	if (ImGui::IsItemClicked())
	{
		SelectedBoneIndex = BoneIndex;
	}

	if (bOpen)
	{
		for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(PreviewBones.size()); ++ChildIndex)
		{
			if (PreviewBones[ChildIndex].ParentIndex == BoneIndex)
			{
				RenderBoneNode(ChildIndex);
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void FEditorSkeletalMeshViewer::RenderSelectedBoneDetails()
{
	ImGui::BeginChild("SkeletalMeshBoneDetails", ImVec2(0.0f, 0.0f), true);
	ImGui::TextUnformatted("Selected Bone");
	ImGui::Separator();

	if (SelectedBoneIndex < 0 || SelectedBoneIndex >= static_cast<int32>(PreviewBones.size()))
	{
		ImGui::TextDisabled("Select a bone.");
		ImGui::EndChild();
		return;
	}

	FPreviewBone& Bone = PreviewBones[SelectedBoneIndex];
	ImGui::Text("Index: %d", SelectedBoneIndex);
	ImGui::Text("Name: %s", Bone.Name.c_str());
	ImGui::Text("Parent: %d", Bone.ParentIndex);
	ImGui::Separator();

	ImGui::DragFloat3("Local Translation", &Bone.LocalTranslation.X, 0.01f);
	ImGui::DragFloat3("Local Rotation", &Bone.LocalRotation.Pitch, 0.1f);
	ImGui::DragFloat3("Local Scale", &Bone.LocalScale.X, 0.01f, 0.001f, 100.0f);

	ImGui::EndChild();
}

void FEditorSkeletalMeshViewer::RenderSkeletonOverlay(const ImVec2& Origin, const ImVec2& Size)
{
	if (Size.x <= 0.0f || Size.y <= 0.0f)
	{
		return;
	}

	TArray<FVector> ComponentPositions;
	ComponentPositions.resize(PreviewBones.size(), FVector::ZeroVector);
	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(PreviewBones.size()); ++BoneIndex)
	{
		const FPreviewBone& Bone = PreviewBones[BoneIndex];
		FVector Position = Bone.LocalTranslation;
		if (Bone.ParentIndex >= 0 && Bone.ParentIndex < BoneIndex)
		{
			Position = ComponentPositions[Bone.ParentIndex] + Bone.LocalTranslation;
		}
		ComponentPositions[BoneIndex] = Position;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImU32 LineColor = IM_COL32(92, 134, 180, 255);
	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(PreviewBones.size()); ++BoneIndex)
	{
		const int32 ParentIndex = PreviewBones[BoneIndex].ParentIndex;
		if (ParentIndex < 0 || ParentIndex >= static_cast<int32>(ComponentPositions.size()))
		{
			continue;
		}

		const ImVec2 ParentPos = ProjectBonePosition(ComponentPositions[ParentIndex], Origin, Size);
		const ImVec2 ChildPos = ProjectBonePosition(ComponentPositions[BoneIndex], Origin, Size);
		DrawList->AddLine(ParentPos, ChildPos, LineColor, 2.0f);
	}

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(ComponentPositions.size()); ++BoneIndex)
	{
		const bool bSelected = BoneIndex == SelectedBoneIndex;
		const ImVec2 Pos = ProjectBonePosition(ComponentPositions[BoneIndex], Origin, Size);
		const float Radius = bSelected ? 6.0f : 4.0f;
		DrawList->AddCircleFilled(Pos, Radius, BoneColor(bSelected), 18);
		if (bSelected)
		{
			DrawList->AddCircle(Pos, Radius + 4.0f, EditorAccentColor::ToU32(), 24, 2.0f);
		}
	}
}
