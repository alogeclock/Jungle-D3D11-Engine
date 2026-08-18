#include "Source/Editor/Public/EditorSpriteActor.h"
#include "Source/Editor/Public/EditorViewportClient.h"

AEditorSpriteActor::AEditorSpriteActor(const FString& InString) : AActor(InString)
{
    USceneComponent* Root = new USceneComponent("EditorSpriteRootComponent");
    this->SetRootComponent(Root);
    AddOwnedComponent(Root);
    Root->RegisterComponent();

    SpriteComponent = new UBillboardComponent("EditorBillboardComponent");
    SpriteComponent->SetTexturePath("Data/Texture/EmptyActor.dds");

    SpriteComponent->SetOuter(this);
    SpriteComponent->RegisterComponent();
    SpriteComponent->SetIsInEditor(true);
    SpriteComponent->SetCullMode(ECullMode::None);
    SpriteComponent->SetEnableDepthTest(false);
    SpriteComponent->SetVisible(false);
}

AEditorSpriteActor::~AEditorSpriteActor()
{
}

void AEditorSpriteActor::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
    // 마지막 선택된 컴포넌트의 액터의 SceneComponent의 위치에 스프라이트 컴포넌트를 배치

    USelection* Selection = nullptr;
    if (GEditor)
    {
        Selection = GEditor->GetSelection();
    }
    if (!Selection || Selection->IsEmpty())
    {
        SpriteComponent->SetVisible(false);
        return;
    }

    // PivotComponent는 가장 마지막에 선택된 Actor또는 Component의 RootComponent
    UObject* SelectedObj = Selection->GetSelectedObject(Selection->GetCount() - 1);
    USceneComponent* PivotComponent = nullptr;

    if (UActorComponent* SelectedComponent = Cast<UActorComponent>(SelectedObj))
    {
        if (AActor* Owner = SelectedComponent->GetOwner())
        {
            PivotComponent = Owner->GetRootComponent();
        }
    }
    else if (AActor* SelectedActor = Cast<AActor>(SelectedObj))
    {
        PivotComponent = SelectedActor->GetRootComponent();
    }

    if (PivotComponent)
    {
        FTransform TargetTransform = PivotComponent->GetTransform();

        FEditorViewportClient* ViewportClient = GEditor->GetEditorViewportClient();
        if (ViewportClient)
        {
            FViewportCameraTransform Camera = ViewportClient->GetCameraTransform();
            SpriteComponent->ApplyBillboardTransform(TargetTransform, Camera);
        }

        SpriteComponent->SetVisible(true);
    }
    else
    {
         SpriteComponent->SetVisible(false);
    }
}
