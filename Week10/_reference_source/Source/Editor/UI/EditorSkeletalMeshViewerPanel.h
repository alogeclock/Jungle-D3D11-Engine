#pragma once

#include "Editor/UI/EditorPanel.h"

struct FBorn
{
	
};

class FSkeletalMeshEditor;
class USkeletalMesh;

class FEditorSkeletalMeshViewerPanel : public FEditorPanel
{
public:
    void Initialize(UEditorEngine* InEditorEngine, FSkeletalMeshEditor* InOwner)
    {
        FEditorPanel::Initialize(InEditorEngine);
        Owner = InOwner;
    }

    // FEditorPanel을(를) 통해 상속됨
    void Render(float DeltaTime);

    void Release();
    void Update();

	void RenderToolbar();
    void RenderPreviewViewport(float DeltaTime);
	void RenderBoneHierarchyTree();
    void RenderBoneNode(FBorn* RootBone);
	void RenderSelectedBoneTransformInspector();

	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh) { PreviewSkeletalMesh = InSkeletalMesh;}

private:
    FSkeletalMeshEditor* Owner = nullptr;
	//FSkeletalPreviewViewportClient PreviewClient;

    // 실제 SkeletalMesh 참조
    USkeletalMesh* SkeletalMesh = nullptr;
	//실제 SkeletalMesh가 아닌 복제
    USkeletalMesh* PreviewSkeletalMesh = nullptr;

    FBorn* SelectedBone = nullptr;
    bool bShowMesh = true;
    bool bShowSkeleton = true;
    bool bShowBoneNames = false;
};
