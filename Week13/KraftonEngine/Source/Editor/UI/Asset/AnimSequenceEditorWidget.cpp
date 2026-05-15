#include "AnimSequenceEditorWidget.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Editor/Settings/EditorSettings.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "GameFramework/WorldContext.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Runtime/Engine.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>

namespace
{
	// Value값을 String으로 포매팅 + 3자리 마다 ,로 구분시키는 hepler
	FString FormatAnimSequenceStatCount(size_t Value)
	{
		FString Result = std::to_string(Value);
		for (int32 InsertPos = static_cast<int32>(Result.length()) - 3; InsertPos > 0; InsertPos -= 3)
		{
			Result.insert(static_cast<size_t>(InsertPos), ",");
		}
		return Result;
	}
}

// 다중 Sequence Instance 처리
static uint32 GNextAnimSequenceEditorInstanceId = 0;

FAnimSequenceEditorWidget::FAnimSequenceEditorWidget()
{
	const FString Id = std::to_string(GNextAnimSequenceEditorInstanceId++);
	PreviewWorldHandle = FName("AnimSequenceEditorPreview_" + Id);
	WindowIdSuffix = "###AnimSequenceEditor_" + Id;
}

bool FAnimSequenceEditorWidget::CanEdit(UObject* Object) const
{
	return Object;//&& Object->IsA<UAnimSequence>();
}

bool FAnimSequenceEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	/*const UAnimSequence* CurrentSequence = Cast<UAnimSequence>(EditedObject);
	const UAnimSequence* RequestedSequence = Cast<UAnimSequence>(Object);

	if (!IsOpen() || !CurrentSequence || !RequestedSequence)
	{
		return false;
	}

	const FString& CurrentPath = CurrentSequence->GetAssetPathFileName();
	return !CurrentPath.empty()
		&& CurrentPath != "None"
		&& CurrentPath == RequestedSequence->GetAssetPathFileName();*/

	return true;
}

void FAnimSequenceEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	//AnimSequence = Cast<UAnimSequence>(EditedObject);
	//PreviewSkeletalMesh = AnimSequence ? AnimSequence->GetPreviewSkeletalMesh() : nullptr;
	//PreviewMeshComponent = nullptr;
	//SelectedBoneIndex = -1;

	//if (AnimSequence)
	//{
	//	PlayLength = AnimSequence->GetPlayLength();

	//	CurrentTime = 0.0f;
	//	PreviousTime = 0.0f;
	//	bPlaying = false;
	//	bLooping = true;

	//	const float Padding = std::max(0.25f, PlayLength * 0.08f);
	//	ViewStartTime = -Padding;
	//	ViewEndTime = std::max(PlayLength + Padding, 1.0f);

	//	SelectedNotifyIndex = -1;
	//	DraggingNotifyIndex = -1;
	//	bDraggingNotify = false;
	//	ContextTimelineTime = 0.0f;

	//	PreviewNotifyMarkers.clear();
	//	EditorBoneOverrides.clear();
	//}

	//if (!AnimSequence || !PreviewSkeletalMesh)
	//{
	//	// TODO(AnimationSequenceEditor): AnimSequence가 Preview SkeletalMesh를 직접 제공하도록 연결되면
	//	// 이 빈 Preview 상태 대신 asset 참조를 로드한다.
	//	return;
	//}
	// World Type을 Preview로 변경
	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	// SkeletalMesh를 Component로 가지는 Actor 생성
	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	PreviewMeshComponent = Actor->AddComponent<USkeletalMeshComponent>();
	PreviewMeshComponent->SetSkeletalMesh(PreviewSkeletalMesh);
	Actor->SetRootComponent(PreviewMeshComponent);
	Actor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	// Directional Light
	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>();
	if (LightComp)
	{
		LightComp->SetShadowBias(0.0f);
		LightComp->PushToScene();
	}

	// Floor
	AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	FloorActor->InitDefaultComponents("Data/BasicShape/Cube.OBJ");
	FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
	FloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.02f));

	// Viewport Client
	// TODO: 지금은 Mesh Viewport Client하고 다른점이 없기 때문에 MeshViewportClient를 사용한다.
	// 추후 문제 생기면 AnimSequenceEditorWidget 전용 ViewportClient 추가
	const ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), ViewportSize.x, ViewportSize.y);
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(Actor->GetComponentByClass<USkeletalMeshComponent>());

	ViewportClient.CreatePreviewGizmo();
	ViewportClient.CreateBoneDebugComponent();
	ViewportClient.ResetCameraToPreviousBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	ViewportClient.SetSelectedBone(Cast<USkeletalMesh>(EditedObject), -1);

	FSlateApplication::Get().RegisterViewport(&MeshViewportWindow, &ViewportClient);
}

void FAnimSequenceEditorWidget::Close()
{
	FAssetEditorWidget::Close();

	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);

	ViewportClient.Release();

	// AnimSequence = nullptr;
	PreviewSkeletalMesh = nullptr;
	PreviewMeshComponent = nullptr;
	SelectedBoneIndex = -1;
	EditorBoneOverrides.clear();
	PreviewNotifyMarkers.clear();
}

void FAnimSequenceEditorWidget::Tick(float DeltaTime)
{
	/*if (!IsOpen() || !AnimSequence)
	{
		return;
	}*/
	if (!IsOpen()) return;

	TickTimeline(DeltaTime);

	// TODO(Animation): AnimSequence Pose 평가가 구현되면 여기서 먼저 적용한다.
	// ApplyAnimationPose(CurrentTime);

	// Editor에서 수정한 Bone Transform은 원본 Pose 위에 다시 덮는다.
	ApplyEditorBoneOverrides();

	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}
}

void FAnimSequenceEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	// ImGui::Image가 들고 있는 SRV lifetime 문제를 피하려고 close는 다음 frame에 수행한다.
	if (bPendingClose)
	{
		Close();
		bPendingClose = false;
		return;
	}

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	static float HierarchyWidth = 250.0f;
	static float DetailsWidth = 300.0f;

	bool bWindowOpen = true;
	FString VisibleTitle = "Animation Sequence Editor";
	/*if (AnimSequence)
	{
		const FString& AssetPath = AnimSequence->GetAssetPathFileName();
		if (!AssetPath.empty())
		{
			VisibleTitle += " - ";
			VisibleTitle += AssetPath;
		}
	}*/
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	const FString WindowTitle = VisibleTitle + WindowIdSuffix;
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			bPendingClose = true;
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ViewportClient.IsRenderable())
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	const float MainHeight = std::max(220.0f, ImGui::GetContentRegionAvail().y - 132.0f);
	ImGui::BeginChild("##AnimSequenceMainArea", ImVec2(0.0f, MainHeight), false);

	ImGui::BeginChild("SkeletonTree", ImVec2(HierarchyWidth, 0), true);
	ImGui::Text("Skeleton Tree");
	ImGui::Separator();

	if (PreviewSkeletalMesh)
	{
		const FSkeletalMesh* Asset = PreviewSkeletalMesh->GetSkeletalMeshAsset();
		if (Asset)
		{
			for (int32 i = 0; i < static_cast<int32>(Asset->Bones.size()); ++i)
			{
				if (Asset->Bones[i].ParentIndex == -1)
				{
					RenderSkeletonTree(Asset, i);
				}
			}
		}
	}
	else
	{
		ImGui::TextDisabled("No preview skeleton.");
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

	ImGui::Button("##AnimSequenceSplitter", ImVec2(4.0f, -1.0f));
	if (ImGui::IsItemActive())
	{
		HierarchyWidth += ImGui::GetIO().MouseDelta.x;
		HierarchyWidth = std::clamp(HierarchyWidth, 100.0f, ImGui::GetWindowWidth() - 100.0f);
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}

	ImGui::PopStyleColor(3);
	ImGui::SameLine();

	RenderViewportPanel(DeltaTime);

	ImGui::SameLine();

	ImGui::BeginChild("AnimSequenceBoneDetails", ImVec2(DetailsWidth, 0), true);
	RenderBoneDetailsPanel();
	ImGui::EndChild();

	ImGui::EndChild();

	ImGui::Separator();
	RenderTimelinePanel();

	ImGui::End();

	if (!bWindowOpen)
	{
		bPendingClose = true;
	}
}

void FAnimSequenceEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen() && ViewportClient.IsRenderable())
	{
		OutClients.push_back(const_cast<FMeshEditorViewportClient*>(&ViewportClient));
	}
}

void FAnimSequenceEditorWidget::RenderSkeletonTree(const FSkeletalMesh* Asset, int32 BoneIndex)
{
	const FBone& Bone = Asset->Bones[BoneIndex];

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen;

	if (BoneIndex == SelectedBoneIndex)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	bool bHasChildren = false;
	for (int32 i = BoneIndex + 1; i < static_cast<int32>(Asset->Bones.size()); ++i)
	{
		if (Asset->Bones[i].ParentIndex == BoneIndex)
		{
			bHasChildren = true;
			break;
		}
	}

	if (!bHasChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	const bool bOpen = ImGui::TreeNodeEx(Bone.Name.c_str(), Flags);

	if (ImGui::IsItemClicked())
	{
		SetSelectedBones(BoneIndex);
	}

	if (bOpen && bHasChildren)
	{
		for (int32 i = BoneIndex + 1; i < static_cast<int32>(Asset->Bones.size()); ++i)
		{
			if (Asset->Bones[i].ParentIndex == BoneIndex)
			{
				RenderSkeletonTree(Asset, i);
			}
		}
		ImGui::TreePop();
	}
}

void FAnimSequenceEditorWidget::RenderViewportPanel(float Deltatime)
{
	(void)Deltatime;

	const float DetailsWidth = 300.0f;
	float AvailableWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x;
	AvailableWidth = std::max(1.0f, AvailableWidth);
	const ImVec2 Size(AvailableWidth, ImGui::GetContentRegionAvail().y);
	const ImVec2 ViewportPos = ImGui::GetCursorScreenPos();

	if (!ViewportClient.IsRenderable())
	{
		ImGui::BeginChild("AnimSequenceMissingPreviewViewport", Size, true);
		ImGui::TextDisabled("No Preview Mesh");
		ImGui::Spacing();
		ImGui::TextWrapped("TODO(AnimationSequenceEditor): AnimSequence에서 Preview SkeletalMesh를 찾는 연결이 필요합니다.");
		ImGui::EndChild();
		return;
	}

	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

	FViewport* VP = ViewportClient.GetViewport();
	if (VP && Size.x > 0.0f && Size.y > 0.0f)
	{
		VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));
		MeshViewportWindow.SetRect(FRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y));

		if (VP->GetSRV())
		{
			ImGui::Image((ImTextureID)VP->GetSRV(), Size);
		}
		else
		{
			ImGui::Dummy(Size);
		}

		constexpr float ToolbarHeight = 28.0f;

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(
			ViewportPos,
			ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight),
			IM_COL32(40, 40, 40, 255));

		FViewportToolbarContext Context;
		Context.Renderer = &GEngine->GetRenderer();
		Context.Gizmo = ViewportClient.GetGizmo();
		Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
		Context.RenderOptions = &ViewportClient.GetRenderOptions();
		Context.ToolbarLeft = ViewportPos.x;
		Context.ToolbarTop = ViewportPos.y;
		Context.ToolbarWidth = Size.x;
		Context.bReservePlayStopSpace = false;
		Context.bShowAddActor = false;
		Context.OnCoordSystemToggled = [&]()
		{
			FGizmoToolSettings& Settings = FEditorSettings::Get().MeshEditorViewportSettings.Gizmo;
			Settings.CoordSystem = (Settings.CoordSystem == EEditorCoordSystem::World) ? EEditorCoordSystem::Local : EEditorCoordSystem::World;

			ViewportClient.ApplyTransformSettingsToGizmo();
		};
		Context.OnSettingsChanged = [&]()
		{
			ViewportClient.ApplyTransformSettingsToGizmo();
		};
		Context.OnRenderViewModeExtras = [&]()
		{
			const EBoneDebugDrawMode CurrentBoneDrawMode = ViewportClient.GetBoneDebugDrawMode();
			int32 BoneDrawMode = static_cast<int32>(CurrentBoneDrawMode);
			ImGui::Text("Bone Display");
			ImGui::RadioButton("Selected Bone", &BoneDrawMode, static_cast<int32>(EBoneDebugDrawMode::SelectedOnly));
			ImGui::RadioButton("All Bones", &BoneDrawMode, static_cast<int32>(EBoneDebugDrawMode::AllBones));
			if (BoneDrawMode != static_cast<int32>(CurrentBoneDrawMode))
			{
				ViewportClient.SetBoneDebugDrawMode(static_cast<EBoneDebugDrawMode>(BoneDrawMode));
			}
		};

		FViewportToolbar::Render(Context);
		RenderStatsOverlay(DrawList, ViewportPos);
	}
}

void FAnimSequenceEditorWidget::RenderBoneDetailsPanel()
{
	ImGui::Text("Bone Details");
	ImGui::Separator();

	if (!PreviewSkeletalMesh || SelectedBoneIndex == -1)
	{
		ImGui::TextDisabled("Select a bone to edit.");
		return;
	}

	FSkeletalMesh* Asset = PreviewSkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset || SelectedBoneIndex < 0 || SelectedBoneIndex >= static_cast<int32>(Asset->Bones.size()))
	{
		ImGui::TextDisabled("Invalid bone selection.");
		return;
	}

	const FBone& Bone = Asset->Bones[SelectedBoneIndex];

	ImGui::Text("Name: %s", Bone.Name.c_str());
	ImGui::Text("Index: %d", SelectedBoneIndex);
	ImGui::Dummy(ImVec2(0, 10));

	FTransform LocalTransform = PreviewMeshComponent
		? PreviewMeshComponent->GetBoneLocalTransformByIndex(SelectedBoneIndex)
		: FTransform(Bone.LocalMatrix);

	if (ImGui::BeginTable("##AnimSequenceBoneTransformTable", 2, ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Location");
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		FVector Location = LocalTransform.Location;
		if (ImGui::DragFloat3("##AnimSequenceBoneLocation", &Location.X, 0.1f))
		{
			LocalTransform.Location = Location;
			SetEditedBoneTransform(SelectedBoneIndex, LocalTransform);
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Rotation");
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		FVector Rotation = LocalTransform.GetRotator().ToVector();
		if (ImGui::DragFloat3("##AnimSequenceBoneRotation", &Rotation.X, 0.1f))
		{
			LocalTransform.SetRotation(FRotator(Rotation));
			SetEditedBoneTransform(SelectedBoneIndex, LocalTransform);
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Scale");
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
		FVector Scale = LocalTransform.Scale;
		if (ImGui::DragFloat3("##AnimSequenceBoneScale", &Scale.X, 0.1f, 0.01f))
		{
			LocalTransform.Scale = Scale;
			SetEditedBoneTransform(SelectedBoneIndex, LocalTransform);
		}

		ImGui::EndTable();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("TODO(AnimationSequenceEditor):");
	ImGui::TextDisabled("Bone override 저장은 구현하지 않는다.");
}

void FAnimSequenceEditorWidget::RenderStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const
{
	if (!DrawList)
	{
		return;
	}

	size_t VertexCount = 0;
	size_t TriangleCount = 0;
	size_t BoneCount = 0;

	if (PreviewSkeletalMesh)
	{
		if (const FSkeletalMesh* Asset = PreviewSkeletalMesh->GetSkeletalMeshAsset())
		{
			VertexCount = Asset->Vertices.size();
			TriangleCount = Asset->Indices.size() / 3;
			BoneCount = Asset->Bones.size();
		}
	}

	const FString Text =
		"Triangles: " + FormatAnimSequenceStatCount(TriangleCount) + "\n" +
		"Vertices: " + FormatAnimSequenceStatCount(VertexCount) + "\n" +
		"Bones: " + FormatAnimSequenceStatCount(BoneCount);

	const ImVec2 TextPos(ViewportPos.x + 8.0f, ViewportPos.y + 36.0f);
	DrawList->AddText(ImVec2(TextPos.x + 1.0f, TextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), Text.c_str());
	DrawList->AddText(TextPos, IM_COL32(235, 238, 242, 255), Text.c_str());
}

void FAnimSequenceEditorWidget::TickTimeline(float DeltaTime)
{
	PreviousTime = CurrentTime;

	if (!bPlaying || PlayLength <= 0.0f)
	{
		return;
	}

	CurrentTime += DeltaTime;

	if (bLooping)
	{
		while (CurrentTime > PlayLength)
		{
			CurrentTime -= PlayLength;
		}
	}
	else
	{
		CurrentTime = std::clamp(CurrentTime, 0.0f, PlayLength);

		if (CurrentTime >= PlayLength)
		{
			bPlaying = false;
		}
	}
}

void FAnimSequenceEditorWidget::RenderTimelinePanel()
{
	if (PlayLength <= 0.0f)
	{
		ImGui::TextDisabled("No animation timeline.");
		return;
	}

	constexpr float TimelineHeight = 120.0f;
	constexpr float LeftPanelWidth = 180.0f;
	constexpr float RulerHeight = 28.0f;

	ImGui::BeginChild(
		"##AnimSequenceTimelinePanel",
		ImVec2(0.0f, TimelineHeight),
		true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	if (ImGui::Button(bPlaying ? "Pause" : "Play"))
	{
		bPlaying = !bPlaying;
	}

	ImGui::SameLine();

	if (ImGui::Button("Stop"))
	{
		bPlaying = false;
		PreviousTime = CurrentTime;
		CurrentTime = 0.0f;
	}

	ImGui::SameLine();
	ImGui::Checkbox("Loop", &bLooping);

	ImGui::SameLine();

	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::DragFloat("Time", &CurrentTime, 0.01f, 0.0f, PlayLength, "%.3f"))
	{
		PreviousTime = CurrentTime;
		CurrentTime = std::clamp(CurrentTime, 0.0f, PlayLength);
	}

	ImGui::SameLine();
	ImGui::Text("Length: %.3f", PlayLength);

	const ImVec2 PanelPos = ImGui::GetCursorScreenPos();
	const ImVec2 PanelSize = ImGui::GetContentRegionAvail();
	const ImVec2 PanelEnd(PanelPos.x + PanelSize.x, PanelPos.y + PanelSize.y);

	const float TimelineX = PanelPos.x + LeftPanelWidth;
	const float TimelineWidth = std::max(1.0f, PanelEnd.x - TimelineX);
	const float TrackTop = PanelPos.y + RulerHeight;
	const float VisibleRange = std::max(0.05f, ViewEndTime - ViewStartTime);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	ImGui::InvisibleButton(
		"##AnimSequenceTimelineCanvas",
		PanelSize,
		ImGuiButtonFlags_MouseButtonLeft |
		ImGuiButtonFlags_MouseButtonRight |
		ImGuiButtonFlags_MouseButtonMiddle);

	const bool bCanvasHovered = ImGui::IsItemHovered();
	const bool bCanvasActive = ImGui::IsItemActive();
	const ImVec2 Mouse = ImGui::GetIO().MousePos;

	DrawList->AddRectFilled(PanelPos, PanelEnd, ImGui::GetColorU32(ImGuiCol_WindowBg));
	DrawList->AddRect(PanelPos, PanelEnd, ImGui::GetColorU32(ImGuiCol_Border));

	DrawList->AddText(
		ImVec2(PanelPos.x + 10.0f, TrackTop + 7.0f),
		ImGui::GetColorU32(ImGuiCol_Text),
		"Notifies");

	if (bCanvasHovered && ImGui::GetIO().KeyCtrl && std::fabs(ImGui::GetIO().MouseWheel) > 0.0f)
	{
		const float MouseTime = XToTime(
			std::clamp(Mouse.x, TimelineX, PanelEnd.x),
			TimelineX,
			TimelineWidth,
			VisibleRange);

		const float ZoomFactor = ImGui::GetIO().MouseWheel > 0.0f ? 0.85f : 1.0f / 0.85f;
		const float NewRange = std::clamp(VisibleRange * ZoomFactor, 0.05f, 100000.0f);
		const float AnchorAlpha = std::clamp((MouseTime - ViewStartTime) / VisibleRange, 0.0f, 1.0f);

		ViewStartTime = MouseTime - NewRange * AnchorAlpha;
		ViewEndTime = ViewStartTime + NewRange;
	}

	if (bCanvasActive &&
		(ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
		 ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
	{
		const float DeltaTime = -(ImGui::GetIO().MouseDelta.x / TimelineWidth) * VisibleRange;
		ViewStartTime += DeltaTime;
		ViewEndTime += DeltaTime;
	}

	const bool bMouseInRuler =
		bCanvasHovered &&
		Mouse.x >= TimelineX &&
		Mouse.x <= PanelEnd.x &&
		Mouse.y >= PanelPos.y &&
		Mouse.y <= TrackTop;

	if (bMouseInRuler && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		PreviousTime = CurrentTime;

		CurrentTime = XToTime(Mouse.x, TimelineX, TimelineWidth, VisibleRange);
		CurrentTime = std::clamp(CurrentTime, 0.0f, PlayLength);
	}

	const ImU32 GridColor = IM_COL32(160, 160, 160, 80);
	const ImU32 MinorGridColor = IM_COL32(160, 160, 160, 36);

	const float MajorStepCandidates[] =
	{
		0.01f, 0.05f, 0.1f, 0.25f, 0.5f,
		1.0f, 2.0f, 5.0f, 10.0f, 30.0f, 60.0f
	};

	float MajorStep = 1.0f;
	for (float Candidate : MajorStepCandidates)
	{
		if ((Candidate / VisibleRange) * TimelineWidth >= 72.0f)
		{
			MajorStep = Candidate;
			break;
		}
	}

	const float MinorStep = MajorStep * 0.5f;
	const float FirstMinor = std::floor(ViewStartTime / MinorStep) * MinorStep;

	DrawList->PushClipRect(ImVec2(TimelineX, PanelPos.y), PanelEnd, true);

	for (float Time = FirstMinor; Time <= ViewEndTime + MinorStep; Time += MinorStep)
	{
		const float X = TimeToX(Time, TimelineX, TimelineWidth, VisibleRange);
		const bool bMajor = std::fabs(std::fmod(std::fabs(Time), MajorStep)) < 0.0001f;

		DrawList->AddLine(
			ImVec2(X, PanelPos.y),
			ImVec2(X, PanelEnd.y),
			bMajor ? GridColor : MinorGridColor);

		if (bMajor)
		{
			char Label[32];
			std::snprintf(Label, sizeof(Label), "%.2f", Time);

			DrawList->AddText(
				ImVec2(X + 4.0f, PanelPos.y + 6.0f),
				ImGui::GetColorU32(ImGuiCol_TextDisabled),
				Label);
		}
	}

	const float StartX = TimeToX(0.0f, TimelineX, TimelineWidth, VisibleRange);
	const float EndX = TimeToX(PlayLength, TimelineX, TimelineWidth, VisibleRange);

	DrawList->AddLine(
		ImVec2(StartX, PanelPos.y),
		ImVec2(StartX, PanelEnd.y),
		IM_COL32(44, 220, 84, 255),
		2.0f);

	DrawList->AddLine(
		ImVec2(EndX, PanelPos.y),
		ImVec2(EndX, PanelEnd.y),
		IM_COL32(220, 24, 24, 255),
		2.0f);

	DrawList->PopClipRect();

	DrawNotifyTrack(
		DrawList,
		PanelPos,
		PanelEnd,
		TimelineX,
		TrackTop,
		TimelineWidth,
		VisibleRange,
		bCanvasHovered,
		Mouse);

	const float PlayheadX = TimeToX(CurrentTime, TimelineX, TimelineWidth, VisibleRange);

	DrawList->PushClipRect(ImVec2(TimelineX, PanelPos.y), PanelEnd, true);

	DrawList->AddLine(
		ImVec2(PlayheadX, PanelPos.y),
		ImVec2(PlayheadX, PanelEnd.y),
		IM_COL32(255, 255, 255, 255),
		2.0f);

	DrawList->AddRectFilled(
		ImVec2(PlayheadX - 6.0f, PanelPos.y + 2.0f),
		ImVec2(PlayheadX + 6.0f, PanelPos.y + 16.0f),
		IM_COL32(255, 64, 64, 255),
		2.0f);

	DrawList->PopClipRect();

	ImGui::EndChild();
}

void FAnimSequenceEditorWidget::DrawNotifyTrack(
	ImDrawList* DrawList,
	const ImVec2& PanelPos,
	const ImVec2& PanelEnd,
	float TimelineX,
	float TrackTop,
	float TimelineWidth,
	float VisibleRange,
	bool bCanvasHovered,
	const ImVec2& Mouse)
{
	constexpr float NotifyRowHeight = 30.0f;

	const float RowY = TrackTop;
	const float RowMinY = RowY;
	const float RowMaxY = RowY + NotifyRowHeight;

	TArray<FAnimNotifyEvent>& Markers = GetNotifyMarkers();

	DrawList->AddRectFilled(
		ImVec2(PanelPos.x, RowMinY),
		ImVec2(PanelEnd.x, RowMaxY),
		IM_COL32(45, 45, 45, 180));

	DrawList->AddLine(
		ImVec2(PanelPos.x, RowMaxY),
		ImVec2(PanelEnd.x, RowMaxY),
		ImGui::GetColorU32(ImGuiCol_Border));

	int32 HoveredNotifyIndex = -1;

	DrawList->PushClipRect(ImVec2(TimelineX, RowMinY), ImVec2(PanelEnd.x, RowMaxY), true);

	for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(Markers.size()); ++NotifyIndex)
	{
		const FAnimNotifyEvent& Notify = Markers[NotifyIndex];

		const float X = TimeToX(Notify.TriggerTime, TimelineX, TimelineWidth, VisibleRange);
		if (X < TimelineX || X > PanelEnd.x)
		{
			continue;
		}

		const bool bSelected = NotifyIndex == SelectedNotifyIndex;

		const ImU32 MarkerColor = bSelected
			? IM_COL32(255, 230, 90, 255)
			: IM_COL32(220, 70, 70, 255);

		const ImVec2 P0(X, RowY + 5.0f);
		const ImVec2 P1(X - 6.0f, RowY + 18.0f);
		const ImVec2 P2(X + 6.0f, RowY + 18.0f);

		DrawList->AddTriangleFilled(P0, P1, P2, MarkerColor);
		DrawList->AddTriangle(P0, P1, P2, IM_COL32(20, 20, 20, 255), 1.0f);

		const FString NotifyName = Notify.NotifyName.ToString();

		DrawList->AddText(
			ImVec2(X + 8.0f, RowY + 8.0f),
			ImGui::GetColorU32(ImGuiCol_Text),
			NotifyName.c_str());

		const float Dx = Mouse.x - X;
		const float Dy = Mouse.y - (RowY + 13.0f);

		if ((Dx * Dx + Dy * Dy) <= 12.0f * 12.0f)
		{
			HoveredNotifyIndex = NotifyIndex;
		}
	}

	DrawList->PopClipRect();

	const bool bMouseInNotifyRow =
		bCanvasHovered &&
		Mouse.x >= TimelineX &&
		Mouse.x <= PanelEnd.x &&
		Mouse.y >= RowMinY &&
		Mouse.y <= RowMaxY;

	if (bMouseInNotifyRow && HoveredNotifyIndex >= 0)
	{
		const FAnimNotifyEvent& Notify = Markers[HoveredNotifyIndex];
		const FString NotifyName = Notify.NotifyName.ToString();

		ImGui::BeginTooltip();
		ImGui::Text("Notify: %s", NotifyName.c_str());
		ImGui::Text("Time: %.3f", Notify.TriggerTime);
		ImGui::Text("Duration: %.3f", Notify.Duration);
		ImGui::EndTooltip();
	}

	if (bMouseInNotifyRow && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		SelectedNotifyIndex = HoveredNotifyIndex;

		if (HoveredNotifyIndex >= 0)
		{
			bDraggingNotify = true;
			DraggingNotifyIndex = HoveredNotifyIndex;
		}
	}

	if (bDraggingNotify &&
		DraggingNotifyIndex >= 0 &&
		ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		float NewTime = XToTime(Mouse.x, TimelineX, TimelineWidth, VisibleRange);

		if (!ImGui::GetIO().KeyAlt)
		{
			NewTime = SnapTime(NewTime);
		}

		MoveVisualNotify(DraggingNotifyIndex, NewTime);
	}

	if (bDraggingNotify && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		bDraggingNotify = false;
		DraggingNotifyIndex = -1;

		SortVisualNotifyMarkers();
	}

	if (bMouseInNotifyRow &&
		ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
		!ImGui::IsMouseDragging(ImGuiMouseButton_Right, ImGui::GetIO().MouseDragThreshold))
	{
		ContextTimelineTime = XToTime(Mouse.x, TimelineX, TimelineWidth, VisibleRange);
		ContextTimelineTime = SnapTime(ContextTimelineTime);
		ContextTimelineTime = std::clamp(ContextTimelineTime, 0.0f, PlayLength);

		ImGui::OpenPopup("##AnimNotifyMarkerContext");
	}

	if (ImGui::BeginPopup("##AnimNotifyMarkerContext"))
	{
		ImGui::Text("Time: %.3f", ContextTimelineTime);
		ImGui::Separator();

		if (ImGui::MenuItem("Add Notify Marker"))
		{
			AddVisualNotifyAtTime(ContextTimelineTime);
		}

		ImGui::BeginDisabled(SelectedNotifyIndex < 0);
		if (ImGui::MenuItem("Delete Selected Notify"))
		{
			DeleteSelectedVisualNotify();
		}
		ImGui::EndDisabled();

		ImGui::EndPopup();
	}

	if (SelectedNotifyIndex >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		DeleteSelectedVisualNotify();
	}
}

float FAnimSequenceEditorWidget::TimeToX(
	float Time,
	float TimelineX,
	float TimelineWidth,
	float VisibleRange) const
{
	return TimelineX + ((Time - ViewStartTime) / VisibleRange) * TimelineWidth;
}

float FAnimSequenceEditorWidget::XToTime(
	float X,
	float TimelineX,
	float TimelineWidth,
	float VisibleRange) const
{
	return ViewStartTime + ((X - TimelineX) / TimelineWidth) * VisibleRange;
}

float FAnimSequenceEditorWidget::SnapTime(float Time) const
{
	constexpr float SnapUnit = 0.01f;
	return std::round(Time / SnapUnit) * SnapUnit;
}

TArray<FAnimNotifyEvent>& FAnimSequenceEditorWidget::GetNotifyMarkers()
{
	// 현재는 Editor UI 선작업용 임시 배열.
	// TODO(AnimNotify): UAnimSequenceBase::Notifies가 병합되면
	// return AnimSequence->GetNotifies(); 로 교체한다.
	return PreviewNotifyMarkers;
}

void FAnimSequenceEditorWidget::AddVisualNotifyAtTime(float Time)
{
	FAnimNotifyEvent NewNotify;
	NewNotify.TriggerTime = std::clamp(Time, 0.0f, PlayLength);
	NewNotify.Duration = 0.0f;
	NewNotify.NotifyName = FName("NewNotify");

	TArray<FAnimNotifyEvent>& Markers = GetNotifyMarkers();

	Markers.push_back(NewNotify);
	SelectedNotifyIndex = static_cast<int32>(Markers.size()) - 1;

	// TODO(AnimNotify): 저장 기능은 UAnimSequenceBase::Notifies 병합 후 구현한다.
	MarkDirty();
}

void FAnimSequenceEditorWidget::MoveVisualNotify(int32 NotifyIndex, float NewTime)
{
	TArray<FAnimNotifyEvent>& Markers = GetNotifyMarkers();

	if (NotifyIndex < 0 || NotifyIndex >= static_cast<int32>(Markers.size()))
	{
		return;
	}

	Markers[NotifyIndex].TriggerTime = std::clamp(NewTime, 0.0f, PlayLength);
	MarkDirty();
}

void FAnimSequenceEditorWidget::DeleteSelectedVisualNotify()
{
	TArray<FAnimNotifyEvent>& Markers = GetNotifyMarkers();

	if (SelectedNotifyIndex < 0 ||
		SelectedNotifyIndex >= static_cast<int32>(Markers.size()))
	{
		return;
	}

	Markers.erase(Markers.begin() + SelectedNotifyIndex);

	SelectedNotifyIndex = -1;
	DraggingNotifyIndex = -1;
	bDraggingNotify = false;

	// TODO(AnimNotify): 저장 기능은 UAnimSequenceBase::Notifies 병합 후 구현한다.
	MarkDirty();
}

void FAnimSequenceEditorWidget::SortVisualNotifyMarkers()
{
	TArray<FAnimNotifyEvent>& Markers = GetNotifyMarkers();

	const bool bHadSelection =
		SelectedNotifyIndex >= 0 &&
		SelectedNotifyIndex < static_cast<int32>(Markers.size());
	const FAnimNotifyEvent SelectedNotify = bHadSelection ? Markers[SelectedNotifyIndex] : FAnimNotifyEvent();

	std::sort(
		Markers.begin(),
		Markers.end(),
		[](const FAnimNotifyEvent& A, const FAnimNotifyEvent& B)
		{
			return A.TriggerTime < B.TriggerTime;
		});

	if (!bHadSelection)
	{
		return;
	}

	for (int32 NotifyIndex = 0; NotifyIndex < static_cast<int32>(Markers.size()); ++NotifyIndex)
	{
		const FAnimNotifyEvent& Notify = Markers[NotifyIndex];
		if (Notify.TriggerTime == SelectedNotify.TriggerTime &&
			Notify.Duration == SelectedNotify.Duration &&
			Notify.NotifyName == SelectedNotify.NotifyName)
		{
			SelectedNotifyIndex = NotifyIndex;
			return;
		}
	}

	SelectedNotifyIndex = -1;
}

void FAnimSequenceEditorWidget::SetSelectedBones(int32 BoneIndex)
{
	SelectedBoneIndex = BoneIndex;
	ViewportClient.SetSelectedBone(PreviewSkeletalMesh, BoneIndex);
}

void FAnimSequenceEditorWidget::SetEditedBoneTransform(
	int32 BoneIndex,
	const FTransform& LocalTransform)
{
	if (BoneIndex < 0 || !PreviewMeshComponent)
	{
		return;
	}

	bool bFound = false;

	for (FEditorBoneOverride& Override : EditorBoneOverrides)
	{
		if (Override.BoneIndex == BoneIndex)
		{
			Override.LocalTransform = LocalTransform;
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		FEditorBoneOverride NewOverride;
		NewOverride.BoneIndex = BoneIndex;
		NewOverride.LocalTransform = LocalTransform;
		EditorBoneOverrides.push_back(NewOverride);
	}

	PreviewMeshComponent->SetBoneLocalTransformByIndex(BoneIndex, LocalTransform);
}

void FAnimSequenceEditorWidget::ApplyEditorBoneOverrides()
{
	if (!PreviewMeshComponent)
	{
		return;
	}

	for (const FEditorBoneOverride& Override : EditorBoneOverrides)
	{
		if (Override.BoneIndex < 0)
		{
			continue;
		}

		PreviewMeshComponent->SetBoneLocalTransformByIndex(
			Override.BoneIndex,
			Override.LocalTransform);
	}
}
