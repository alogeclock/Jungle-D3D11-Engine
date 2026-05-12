#include "SkeletalMeshPreviewWidget.h"

#include "Component/SkeletalMeshComponent.h"
#include "Core/Notification.h"
#include "Editor/EditorEngine.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Texture/Texture2D.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

#include <filesystem>
#include <string>

namespace
{
    // Data Access & Validation
    FSkeletalMesh* GetSkeletalMeshAsset(USkeletalMesh* Mesh);
    const FSkeleton* GetSkeleton(USkeletalMesh* Mesh);
    bool IsValidBoneIndex(const FSkeleton* Skeleton, int32 BoneIndex);

    // Math & Transform
    FQuat NormalizeQuat(const FQuat& Quat);
    bool IsSameRotation(const FQuat& A, const FQuat& B);

    // Editor Window 기반 Rendering
    void SelectBone(FSkeletalMeshPreviewViewportClient& ViewportClient, int32& SelectedBoneIndex, int32 BoneIndex);

    // Text Formatting & UI Constants
    FString GetMeshDisplayName(USkeletalMesh* Mesh);
}

// UEditorEngine, DirectX Device, Windows Handle을 받아 초기화합니다.
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
    SetPreviewViewportClient(&ViewportClient);
    bInitialized = true;
}

// 사용하던 뷰포트를 릴리즈하고 메모리에서 해제한 뒤, 포인터를 nullptr 초기화하여 댕글링 포인터 문제를 방지합니다.
void FSkeletalMeshPreviewWidget::Shutdown()
{
    Close();

    ViewportClient.SetViewport(nullptr);
    SetPreviewViewportClient(nullptr);

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

// 새로운 스켈레탈 메시를 열 때 호출되어, 메시를 연결하고, 본 계층 구조를 분석하며 뷰포트에 렌더링할 대상을 설정합니다.
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
    RegisterPreviewClient(&ViewportClient);
}

// ImGui::BeginTable을 사용하여 Skeleton Tree, Viewport, Details 칸으로 화면을 3분할해 출력합니다.
void FSkeletalMeshPreviewWidget::Render(float DeltaTime)
{
	// 예외 처리 및 윈도우 크기 조정, 타이틀바 렌더링 로직
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
        SetNextPreviewEditorWindowPolicy();
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

	// 화면을 Bone Hierarchy Panel, Viewport Widget, Bone Details Panel로 3등분합니다.
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

// 입력 정보를 초기화합니다.
// 편집 중인 데이터를 초기화하고, 메시 연결을 끊고, 본 인덱스를 리셋하며 등록된 프리뷰 클라이언트를 해제합니다.
void FSkeletalMeshPreviewWidget::Close()
{
    if (!bOpen && !bRegistered && !EditingMesh)
    {
        return;
    }

    UnregisterPreviewClient(&ViewportClient);
    ClearInputCapture();
    ViewportClient.SetSkeletalMesh(nullptr);
    EditingMesh = nullptr;
    BoneWeightedFlags.clear();
    SelectedBoneIndex = -1;
    bOpen = false;
    bCapturingInput = false;
}

// EditorEngine에 Preview Viewport Client를 등록합니다.
 

// EditorEngine에 Preview Viewport Client를 해제합니다.
 

// UI Rendering 및 Layout 로직: Bone Hierarchy Panel을 그립니다.
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
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);
    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const FBoneInfo& Bone = Skeleton->Bones[BoneIndex];
        const bool bHasValidParent = Bone.ParentIndex >= 0 && Bone.ParentIndex < BoneCount;
        if (!bHasValidParent)
        {
            DrawBoneTreeNode(*Skeleton, BoneIndex);
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

// UI Rendering 및 Layout 로직: Bone Details Panel을 그립니다.
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
    USkeletalMeshComponent* PreviewComponent = ViewportClient.GetPreviewComponent();
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

// UI Rendering 및 Layout 로직: FEditorPropertyWidget과 동일한 방식으로 Details Panel을 채웁니다. 
void FSkeletalMeshPreviewWidget::DrawPreviewComponentDetailsPanel()
{
    USkeletalMeshComponent* PreviewComponent = ViewportClient.GetPreviewComponent();
    ImGui::Dummy(ImVec2(0.0f, PreviewDetailsPropertyVerticalSpacing));
    if (!BeginPreviewDetailsSection("SkeletalMeshComponent"))
    {
        return;
    }

    ComponentDetailsWidget.RenderComponentDetails(PreviewComponent, false);
}

// UI Rendering 및 Layout 로직: 트리 창에서 개별 본을 아이콘과 함께 그립니다.
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
    ImGui::SameLine(0.0f, 0.0f);
    if (BoneIcon && BoneIcon->IsLoaded())
    {
        ImGui::Image(BoneIcon->GetSRV(), ImVec2(16.0f, 16.0f));
        const bool bIconClicked = ImGui::IsItemClicked();
        if (bIconClicked)
        {
            SelectBone(ViewportClient, SelectedBoneIndex, BoneIndex);
        }
        ImGui::SameLine(0.0f, 2.0f);
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

bool FSkeletalMeshPreviewWidget::DrawEditableTransform(const char* SectionName, FTransform& Transform, int32 BoneIndex, FBoneRotationEditState& State, EBoneRotationEditSpace Space)
{
    if (!BeginPreviewDetailsSection(SectionName))
    {
        return false;
    }

    bool bChanged = false;

    // 1. Location
    float LocValues[3] = { Transform.Location.X, Transform.Location.Y, Transform.Location.Z };
    if (DrawPreviewColoredFloat3("Location", LocValues, 0.1f))
    {
        Transform.Location = FVector(LocValues[0], LocValues[1], LocValues[2]);
        bChanged = true;
    }

    // 2. Rotation
    // 상태 동기화: 본이 바뀌었거나 외부(Gizmo 등)에서 회전값이 바뀐 경우 EulerHint를 갱신합니다.
    const FQuat CurrentQuat = Transform.Rotation;
    if (State.BoneIndex != BoneIndex || !IsSameRotation(State.Quat, CurrentQuat))
    {
        State.BoneIndex = BoneIndex;
        State.Quat = CurrentQuat;
        State.EulerHint = CurrentQuat.ToRotator();
    }

    // Roll(X), Pitch(Y), Yaw(Z) 순서로 표시 (UE 컨벤션)
    float RotValues[3] = { State.EulerHint.Roll, State.EulerHint.Pitch, State.EulerHint.Yaw };
    if (DrawPreviewColoredFloat3("Rotation", RotValues, 0.1f))
    {
        // Pitch, Yaw, Roll 순서로 FRotator 생성자 호출
        State.EulerHint = FRotator(RotValues[1], RotValues[2], RotValues[0]);
        State.Quat = State.EulerHint.ToQuaternion();
        Transform.Rotation = State.Quat;
        bChanged = true;
    }

    // 3. Scale
    float ScaleValues[3] = { Transform.Scale.X, Transform.Scale.Y, Transform.Scale.Z };
    static float DefaultScale[3] = { 1.0f, 1.0f, 1.0f };
    if (DrawPreviewColoredFloat3("Scale", ScaleValues, 0.01f, DefaultScale))
    {
        Transform.Scale = FVector(ScaleValues[0], ScaleValues[1], ScaleValues[2]);
        bChanged = true;
    }

    return bChanged;
}

// 메시의 모든 정점을 순회하며 어떤 본이 실제로 정점에 영향을 주는지 체크합니다.
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

// UI Rendering 및 Layout 로직: 본 아이콘을 불러와서 띄웁니다.
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

// 3D Viewport에서 마우스 클릭이 발생했을 때, 기즈모를 클릭했는지 본을 클릭했는지 판별해 선택 상태를 갱신합니다.
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

// 현재 선택된 본이 유효한지 확인하여, 잘못된 인덱스를 참조하는 것을 방지합니다.
void FSkeletalMeshPreviewWidget::ValidateSelectedBone()
{
    const FSkeleton* Skeleton = GetSkeleton(EditingMesh);
    if (USkeletalMeshComponent* PreviewComponent = ViewportClient.GetPreviewComponent())
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
        if (USkeletalMeshComponent* PreviewComponent = ViewportClient.GetPreviewComponent())
        {
            PreviewComponent->SetSelectedBoneIndex(-1);
        }
    }
}

namespace
{
    // ────────────────────────────────────────────────────────────
    // Data Access & Validation
    // ────────────────────────────────────────────────────────────

	// 전달받은 메시 객체로부터 FSkeletalMesh 에셋을 추출합니다.
    FSkeletalMesh* GetSkeletalMeshAsset(USkeletalMesh* Mesh)
    {
        return Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    }

	// 전달받은 메시 객체로부터 FSkeleton 에셋을 을 추출합니다.
    const FSkeleton* GetSkeleton(USkeletalMesh* Mesh)
    {
        FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(Mesh);
        return MeshAsset ? &MeshAsset->Skeleton : nullptr;
    }
	
	// 입력된 Bone Index가 스켈레톤의 유효 범위 안에 있는지 확인합니다.
    bool IsValidBoneIndex(const FSkeleton* Skeleton, int32 BoneIndex)
    {
        return Skeleton && BoneIndex >= 0 && BoneIndex < static_cast<int32>(Skeleton->Bones.size());
    }

    // ────────────────────────────────────────────────────────────
    // Math & Transform
    // ────────────────────────────────────────────────────────────

	// 상수 참조자로 받은 쿼터니언을 수정 없이 정규화하기 위해 필요한 헬퍼 함수
    FQuat NormalizeQuat(const FQuat& Quat)
    {
        FQuat Result = Quat;
        Result.Normalize();
        return Result;
    }

	// 두 쿼터니언을 정규화한 뒤 동일한지 비교합니다.
    bool IsSameRotation(const FQuat& A, const FQuat& B)
    {
        const FQuat NormalizedA = NormalizeQuat(A);
        const FQuat NormalizedB = NormalizeQuat(B);
        const FQuat NegatedB(-NormalizedB.X, -NormalizedB.Y, -NormalizedB.Z, -NormalizedB.W);
        return NormalizedA.Equals(NormalizedB, 1.0e-4f) || NormalizedA.Equals(NegatedB, 1.0e-4f);
    }

    // ────────────────────────────────────────────────────────────
    // Editor Window 기반 Rendering
    // ────────────────────────────────────────────────────────────

	// 뷰포트와 UI 양쪽에서 본 선택 상태를 동기화합니다.
    void SelectBone(FSkeletalMeshPreviewViewportClient& ViewportClient, int32& SelectedBoneIndex, int32 BoneIndex)
    {
        SelectedBoneIndex = BoneIndex;
        if (USkeletalMeshComponent* PreviewComponent = ViewportClient.GetPreviewComponent())
        {
            PreviewComponent->SetSelectedBoneIndex(BoneIndex);
            SelectedBoneIndex = PreviewComponent->GetSelectedBoneIndex();
        }
    }
	
	// 파일 경로에서 파일명을 추출해 에디터 창의 제목을 결정하는 유틸리티 함수
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
}
