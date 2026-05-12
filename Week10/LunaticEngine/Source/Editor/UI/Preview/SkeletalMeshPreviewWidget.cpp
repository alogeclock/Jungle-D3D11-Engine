#include "SkeletalMeshPreviewWidget.h"

#include "Component/SkeletalMeshComponent.h"
#include "Core/Notification.h"
#include "Editor/EditorEngine.h"
#include "Math/Rotator.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Texture/Texture2D.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>

namespace
{
	enum class EBoneRotationEditSpace : uint8
	{
		Local,
		Component
	};

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

	FQuat NormalizeQuat(const FQuat& Quat)
	{
		FQuat Result = Quat;
		Result.Normalize();
		return Result;
	}

	bool IsSameRotation(const FQuat& A, const FQuat& B)
	{
		const FQuat NormalizedA = NormalizeQuat(A);
		const FQuat NormalizedB = NormalizeQuat(B);
		const FQuat NegatedB(-NormalizedB.X, -NormalizedB.Y, -NormalizedB.Z, -NormalizedB.W);
		return NormalizedA.Equals(NormalizedB, 1.0e-4f) || NormalizedA.Equals(NegatedB, 1.0e-4f);
	}

	void SyncBoneRotationEditState(int32 BoneIndex, const FQuat& CurrentQuat, FBoneRotationEditState& EditState)
	{
		const FQuat NormalizedQuat = NormalizeQuat(CurrentQuat);
		if (EditState.BoneIndex == BoneIndex && IsSameRotation(EditState.Quat, NormalizedQuat))
		{
			return;
		}

		EditState.BoneIndex = BoneIndex;
		EditState.Quat = NormalizedQuat;
		EditState.EulerHint = NormalizedQuat.ToRotator();
	}

	FQuat ComposeEditedBoneRotation(const FQuat& CurrentQuat, const FRotator& DeltaRotator, EBoneRotationEditSpace EditSpace)
	{
		const FQuat NormalizedCurrent = NormalizeQuat(CurrentQuat);
		const FQuat DeltaQuat = NormalizeQuat(DeltaRotator.ToQuaternion());
		return EditSpace == EBoneRotationEditSpace::Component
			? NormalizeQuat(DeltaQuat * NormalizedCurrent)
			: NormalizeQuat(NormalizedCurrent * DeltaQuat);
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

	bool IsMultiViewportEnabled()
	{
		return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
	}

	void SetNextPreviewEditorWindowPolicy(int32 EditorInstanceId)
	{
		if (!IsMultiViewportEnabled())
		{
			return;
		}

		ImGuiWindowClass WindowClass{};
		WindowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
		WindowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoDecoration | ImGuiViewportFlags_NoTaskBarIcon;
		ImGui::SetNextWindowClass(&WindowClass);

		const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		const float Offset = static_cast<float>((std::max)(0, EditorInstanceId - 1) % 6) * 28.0f;
		ImGui::SetNextWindowPos(ImVec2(MainViewport->Pos.x + 96.0f + Offset, MainViewport->Pos.y + 80.0f + Offset), ImGuiCond_FirstUseEver);
	}

	constexpr float PreviewDetailsPropertyLabelWidth = 124.0f;
	constexpr float PreviewDetailsPropertyVerticalSpacing = 6.0f;

	bool BeginPreviewDetailsSection(const char* SectionName)
	{
		const std::string HeaderId = std::string(SectionName) + "##PreviewDetailsSection";
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.76f, 0.76f, 0.78f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		const bool bOpen = ImGui::CollapsingHeader(HeaderId.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(2);
		return bOpen;
	}

	void PushPreviewDetailsFieldStyle()
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(26.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(33.0f / 255.0f, 33.0f / 255.0f, 33.0f / 255.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(43.0f / 255.0f, 43.0f / 255.0f, 43.0f / 255.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f));
	}

	void PopPreviewDetailsFieldStyle()
	{
		ImGui::PopStyleColor(4);
	}

	bool DrawPreviewLabeledField(const char* Label, const std::function<bool()>& DrawField)
	{
		const float RowStartX = ImGui::GetCursorPosX();
		const float TotalWidth = ImGui::GetContentRegionAvail().x;
		const float LabelTextWidth = ImGui::CalcTextSize(Label).x;
		const ImGuiStyle& Style = ImGui::GetStyle();
		const float DesiredLabelWidth = (std::max)(PreviewDetailsPropertyLabelWidth, LabelTextWidth + Style.ItemSpacing.x + Style.FramePadding.x * 2.0f);
		const float MaxLabelWidth = (std::max)(PreviewDetailsPropertyLabelWidth, TotalWidth * 0.48f);
		const float LabelColumnWidth = (std::min)(DesiredLabelWidth, MaxLabelWidth);

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label);
		ImGui::SameLine(RowStartX + LabelColumnWidth);

		const float FieldWidth = TotalWidth - LabelColumnWidth;
		if (FieldWidth > 0.0f)
		{
			ImGui::SetNextItemWidth(FieldWidth);
		}

		return DrawField();
	}

	void DrawPreviewReadOnlyField(const char* Label, const FString& Value)
	{
		DrawPreviewLabeledField(Label, [&]()
		{
			ImGui::TextDisabled("%s", Value.c_str());
			return false;
		});
	}

	FString FormatPreviewVector(const FVector& Value)
	{
		char Buffer[128] = {};
		snprintf(Buffer, sizeof(Buffer), "%.3f, %.3f, %.3f", Value.X, Value.Y, Value.Z);
		return Buffer;
	}

	bool DrawPreviewColoredFloat3(const char* Label, float Values[3], float Speed, const float* ResetValues = nullptr)
	{
		return DrawPreviewLabeledField(Label, [&]()
		{
			bool bChanged = false;
			const char* AxisLabels[3] = { "X", "Y", "Z" };
			const ImVec4 AxisColors[3] =
			{
				ImVec4(0.86f, 0.24f, 0.24f, 1.0f),
				ImVec4(0.38f, 0.72f, 0.28f, 1.0f),
				ImVec4(0.28f, 0.48f, 0.92f, 1.0f)
			};
			const float ResetButtonWidth = ResetValues ? 48.0f : 0.0f;
			const float AvailableWidth = ImGui::GetContentRegionAvail().x - ResetButtonWidth;
			const float AxisWidth = (std::max)(36.0f, (AvailableWidth - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f);

			ImGui::PushID(Label);
			for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
			{
				if (AxisIndex > 0)
				{
					ImGui::SameLine();
				}

				ImGui::BeginGroup();
				ImGui::PushStyleColor(ImGuiCol_Text, AxisColors[AxisIndex]);
				ImGui::TextUnformatted(AxisLabels[AxisIndex]);
				ImGui::PopStyleColor();
				ImGui::SameLine(0.0f, 4.0f);
				ImGui::SetNextItemWidth((std::max)(20.0f, AxisWidth - 18.0f));
				PushPreviewDetailsFieldStyle();
				char FieldId[8] = {};
				snprintf(FieldId, sizeof(FieldId), "##%s", AxisLabels[AxisIndex]);
				bChanged |= ImGui::DragFloat(FieldId, &Values[AxisIndex], Speed, 0.0f, 0.0f, "%.3f");
				PopPreviewDetailsFieldStyle();
				ImGui::EndGroup();
			}

			if (ResetValues)
			{
				ImGui::SameLine();
				if (ImGui::Button("Reset", ImVec2(ResetButtonWidth, 0.0f)))
				{
					Values[0] = ResetValues[0];
					Values[1] = ResetValues[1];
					Values[2] = ResetValues[2];
					bChanged = true;
				}
			}

			ImGui::PopID();
			return bChanged;
		});
	}

	bool DrawEditableTransform(
		const char* SectionName,
		FTransform& Transform,
		int32 BoneIndex,
		FBoneRotationEditState& RotationEditState,
		EBoneRotationEditSpace RotationEditSpace)
	{
		if (!BeginPreviewDetailsSection(SectionName))
		{
			return false;
		}

		bool bChanged = false;
		static const float ZeroVectorReset[3] = { 0.0f, 0.0f, 0.0f };
		static const float UnitScaleReset[3] = { 1.0f, 1.0f, 1.0f };

		float Location[3] = { Transform.Location.X, Transform.Location.Y, Transform.Location.Z };
		if (DrawPreviewColoredFloat3("Location", Location, 0.05f, ZeroVectorReset))
		{
			Transform.Location = FVector(Location[0], Location[1], Location[2]);
			bChanged = true;
		}

		SyncBoneRotationEditState(BoneIndex, Transform.Rotation, RotationEditState);
		float Rotation[3] = { RotationEditState.EulerHint.Roll, RotationEditState.EulerHint.Pitch, RotationEditState.EulerHint.Yaw };
		if (DrawPreviewColoredFloat3("Rotation", Rotation, 0.1f, ZeroVectorReset))
		{
			const FRotator NewEulerHint(Rotation[1], Rotation[2], Rotation[0]);
			const FRotator DeltaRotator(
				NewEulerHint.Pitch - RotationEditState.EulerHint.Pitch,
				NewEulerHint.Yaw - RotationEditState.EulerHint.Yaw,
				NewEulerHint.Roll - RotationEditState.EulerHint.Roll);

			RotationEditState.Quat = ComposeEditedBoneRotation(RotationEditState.Quat, DeltaRotator, RotationEditSpace);
			RotationEditState.EulerHint = NewEulerHint;
			Transform.SetRotation(RotationEditState.Quat);
			bChanged = true;
		}

		float Scale[3] = { Transform.Scale.X, Transform.Scale.Y, Transform.Scale.Z };
		if (DrawPreviewColoredFloat3("Scale", Scale, 0.01f, UnitScaleReset))
		{
			Transform.Scale = FVector(Scale[0], Scale[1], Scale[2]);
			bChanged = true;
		}

		return bChanged;
	}

	void DrawTransformMatrixRows(const char* SectionName, const FMatrix& Matrix)
	{
		if (!BeginPreviewDetailsSection(SectionName))
		{
			return;
		}

		DrawPreviewReadOnlyField("Location", FormatPreviewVector(Matrix.GetLocation()));
		DrawPreviewReadOnlyField("Rotation", FormatPreviewVector(Matrix.GetEuler()));
		DrawPreviewReadOnlyField("Scale", FormatPreviewVector(Matrix.GetScale()));
	}
}

void FSkeletalMeshPreviewWidget::Initialize(UEditorEngine* InEngine, ID3D11Device* InDevice, FWindowsWindow* InWindow)
{
	if (bInitialized)
	{
		return;
	}

	Engine = InEngine;
	Device = InDevice;
	ComponentDetailsWidget.Initialize(InEngine);

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
	Device = nullptr;
	WeightedBoneIcon = nullptr;
	NonWeightedBoneIcon = nullptr;
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
	RebuildBoneWeightedFlags();
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
	if (PreviewDockId != 0)
	{
		ImGui::SetNextWindowDockID(PreviewDockId, ImGuiCond_Appearing);
	}
	else
	{
		SetNextPreviewEditorWindowPolicy(EditorInstanceId);
	}
	const FString WindowTitle = "Skeletal Mesh Editor - " + GetMeshDisplayName(EditingMesh) + "###SkeletalMeshEditor" + std::to_string(EditorInstanceId);
	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse;
	if (PreviewDockId == 0 && IsMultiViewportEnabled())
	{
		WindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking;
	}
	const bool bVisible = ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags);
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
		ImGui::TableSetupColumn("Skeleton", ImGuiTableColumnFlags_WidthFixed, 264.0f);
		ImGui::TableSetupColumn("Viewport", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 360.0f);

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
	BoneWeightedFlags.clear();
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
		if (BeginPreviewDetailsSection("Bone"))
		{
			ImGui::TextDisabled("Select a bone from the hierarchy.");
		}
		DrawPreviewComponentDetailsPanel();
		ImGui::EndChild();
		return;
	}

	const FBoneInfo& Bone = Skeleton->Bones[SelectedBoneIndex];
	USkeletalMeshComponent* PreviewComponent = GetPreviewComponent(ViewportClient);
	if (BeginPreviewDetailsSection("Bone"))
	{
		DrawPreviewReadOnlyField("Name", Bone.Name.empty() ? FString("<Unnamed Bone>") : Bone.Name);
		DrawPreviewReadOnlyField("Index", std::to_string(SelectedBoneIndex));
		if (IsValidBoneIndex(Skeleton, Bone.ParentIndex))
		{
			const FBoneInfo& ParentBone = Skeleton->Bones[Bone.ParentIndex];
			DrawPreviewReadOnlyField("Parent", (ParentBone.Name.empty() ? FString("<Unnamed Bone>") : ParentBone.Name) + " (" + std::to_string(Bone.ParentIndex) + ")");
		}
		else
		{
			DrawPreviewReadOnlyField("Parent", "None");
		}
		DrawPreviewReadOnlyField("Children", std::to_string(static_cast<int32>(Bone.Children.size())));
	}

	if (PreviewComponent)
	{
		if (const FTransform* LocalTransform = PreviewComponent->GetBoneLocalTransform(SelectedBoneIndex))
		{
			FTransform EditableLocal = *LocalTransform;
			ImGui::PushID("LocalTransform");
			if (DrawEditableTransform("Local Transform", EditableLocal, SelectedBoneIndex, LocalRotationEditState, EBoneRotationEditSpace::Local))
			{
				PreviewComponent->SetBoneLocalTransform(SelectedBoneIndex, EditableLocal);
			}
			ImGui::PopID();
		}

		if (const FTransform* ComponentTransform = PreviewComponent->GetBoneComponentSpaceTransform(SelectedBoneIndex))
		{
			FTransform EditableComponent = *ComponentTransform;
			ImGui::PushID("ComponentTransform");
			if (DrawEditableTransform("Component Transform", EditableComponent, SelectedBoneIndex, ComponentRotationEditState, EBoneRotationEditSpace::Component))
			{
				PreviewComponent->SetBoneComponentSpaceTransform(SelectedBoneIndex, EditableComponent);
			}
			ImGui::PopID();
		}
	}

	DrawTransformMatrixRows("Local Bind Pose", Bone.LocalBindPose);
	DrawTransformMatrixRows("Global Bind Pose", Bone.GlobalBindPose);
	DrawPreviewComponentDetailsPanel();

	ImGui::EndChild();
}

void FSkeletalMeshPreviewWidget::DrawPreviewComponentDetailsPanel()
{
	USkeletalMeshComponent* PreviewComponent = GetPreviewComponent(ViewportClient);
	ImGui::Dummy(ImVec2(0.0f, PreviewDetailsPropertyVerticalSpacing));
	if (!BeginPreviewDetailsSection("SkeletalMeshComponent"))
	{
		return;
	}

	ComponentDetailsWidget.RenderComponentDetails(PreviewComponent, false);
}

void FSkeletalMeshPreviewWidget::DrawBoneTreeNode(const FSkeleton& Skeleton, int32 BoneIndex)
{
	if (!IsValidBoneIndex(&Skeleton, BoneIndex))
	{
		return;
	}

	EnsureBoneTreeIcons();

	const FBoneInfo& Bone = Skeleton.Bones[BoneIndex];
	const bool bSelected = BoneIndex == SelectedBoneIndex;
	const bool bLeaf = Bone.Children.empty();
	const bool bWeighted = BoneIndex < static_cast<int32>(BoneWeightedFlags.size()) && BoneWeightedFlags[BoneIndex];
	UTexture2D* BoneIcon = bWeighted ? WeightedBoneIcon : NonWeightedBoneIcon;
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
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
	const bool bOpenNode = ImGui::TreeNodeEx("##BoneNode", Flags);
	const bool bNodeClicked = ImGui::IsItemClicked();
	ImGui::SameLine(0.0f, 4.0f);
	if (BoneIcon && BoneIcon->IsLoaded())
	{
		ImGui::Image(BoneIcon->GetSRV(), ImVec2(16.0f, 16.0f));
		const bool bIconClicked = ImGui::IsItemClicked();
		if (bIconClicked)
		{
			SelectBone(ViewportClient, SelectedBoneIndex, BoneIndex);
		}
		ImGui::SameLine(0.0f, 4.0f);
	}
	ImGui::TextUnformatted(Label.c_str());
	const bool bLabelClicked = ImGui::IsItemClicked();
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", bWeighted ? "Weighted Bone" : "Non-weighted Bone");
	}

	if (bNodeClicked || bLabelClicked)
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

void FSkeletalMeshPreviewWidget::RebuildBoneWeightedFlags()
{
	BoneWeightedFlags.clear();

	const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(EditingMesh);
	const FSkeleton* Skeleton = MeshAsset ? &MeshAsset->Skeleton : nullptr;
	const FSkeletalMeshLOD* LOD = MeshAsset ? MeshAsset->GetLOD(0) : nullptr;
	const int32 BoneCount = Skeleton ? static_cast<int32>(Skeleton->Bones.size()) : 0;
	if (!LOD || BoneCount <= 0)
	{
		return;
	}

	BoneWeightedFlags.assign(BoneCount, false);
	for (const FSkeletalVertex& Vertex : LOD->Vertices)
	{
		for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
		{
			const int32 WeightedBoneIndex = static_cast<int32>(Vertex.BoneIndices[InfluenceIndex]);
			if (Vertex.BoneWeights[InfluenceIndex] > 0.0f && WeightedBoneIndex >= 0 && WeightedBoneIndex < BoneCount)
			{
				BoneWeightedFlags[WeightedBoneIndex] = true;
			}
		}
	}
}

void FSkeletalMeshPreviewWidget::EnsureBoneTreeIcons()
{
	if (!Device)
	{
		return;
	}

	if (!WeightedBoneIcon)
	{
		WeightedBoneIcon = UTexture2D::LoadFromFile("Asset/Editor/Icons/Common/Bone.png", Device);
	}
	if (!NonWeightedBoneIcon)
	{
		NonWeightedBoneIcon = UTexture2D::LoadFromFile("Asset/Editor/Icons/Common/BoneNonWeighted.png", Device);
	}
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
