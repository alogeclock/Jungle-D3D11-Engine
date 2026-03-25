#include "Source/Editor/Public/EditorEngine.h"
#include "Source/Editor/Public/EditorSpriteActor.h"
#include "Source/Editor/Public/EditorViewportClient.h"
#include "Source/Engine/Public/Classes/Components/UUIDTextComponent.h"

UEditorEngine::UEditorEngine(const FString& InString) : UObject(InString)
{
    Selection = new USelection("EditorSelection");
    Grid = new AGrid("EditorGrid");
    Axis = new AAxis("EditorAxis");
    Gizmo = new APivotTransformGizmo("EditorTransformGizmo");
    EditorSprite = new AEditorSpriteActor("EditorSpriteActor");

    RegisterInputListener(Gizmo);
}

UEditorEngine::~UEditorEngine()
{
    Selection->Clear();
    InputListeners.clear();

    if (EditorSprite)
    {
        delete EditorSprite;
        EditorSprite = nullptr;
    }

    if (Gizmo)
    {
        UnregisterInputListener(Gizmo);
        delete Gizmo;
        Gizmo = nullptr;
    }

    if (Axis)
    {
        delete Axis;
        Axis = nullptr;
    }
    
    if (Grid)
    {
        delete Grid;
        Grid = nullptr;
    }

    if (Selection)
    {
        delete Selection;
        Selection = nullptr;
    }

    if (ViewportClient)
    {
        delete ViewportClient;
        ViewportClient = nullptr;
    }
}

void UEditorEngine::Tick(float DeltaTime, FSceneViewOptions& ViewOptions)
{
    if (ViewportClient)
    {
        // Viewport 객체 포인터는 Application 등에서 넘겨받거나 ViewportClient 내부에 캐싱된 것을 사용
        ViewportClient->Tick(DeltaTime);
    }

    if (Grid)
    {
        Grid->Tick(DeltaTime);
    }

    if (Axis)
    {
        Axis->Tick(DeltaTime);
        Axis->SubmitAllActorComponents(ViewOptions);
    }

    if (Gizmo)
    {
        Gizmo->Tick(DeltaTime);
        Gizmo->SubmitAllActorComponents(ViewOptions);
    }

    if (EditorSprite)
    {
        EditorSprite->Tick(DeltaTime);
        // EditorSprite->SubmitAllActorComponents(ViewOptions);
    }
}

void UEditorEngine::Render(URenderer &renderer)
{
    if (Axis)
    {
        Axis->RenderAllActorComponents(renderer);
    }
    
    if (EditorSprite)
    {
        EditorSprite->RenderAllActorComponents(renderer);
    }

    if (Gizmo)
    {
        renderer.DeviceContext->OMSetRenderTargets(1, &renderer.FrameBufferRTV, renderer.GizmoDepthStencilView);
        Gizmo->RenderAllActorComponents(renderer);
        renderer.DeviceContext->OMSetRenderTargets(1, &renderer.FrameBufferRTV, renderer.DepthStencilView);
    }
}

void UEditorEngine::RegisterInputListener(IViewportInputListener* Listener)
{
    if (Listener)
        InputListeners.push_back(Listener);
}

void UEditorEngine::UnregisterInputListener(IViewportInputListener* Listener)
{
    auto it = std::find(InputListeners.begin(), InputListeners.end(), Listener);
    if (it != InputListeners.end())
        InputListeners.erase(it);
}

bool UEditorEngine::ProcessMouseDown(const FVector<float>& RayOrigin, const FVector<float>& RayDir)
{
    for (auto* Listener : InputListeners)
    {
        if (Listener->OnMouseDown(RayOrigin, RayDir))
            return true; // 기즈모 등이 입력을 소모함
    }
    return false;
}

void UEditorEngine::ProcessMouseMove(const FVector<float>& RayOrigin, const FVector<float>& RayDir)
{
    for (auto* Listener : InputListeners)
        Listener->OnMouseMove(RayOrigin, RayDir);
}

void UEditorEngine::ProcessMouseHover(const FVector<float>& RayOrigin, const FVector<float>& RayDir)
{
    for (auto* Listener : InputListeners)
        Listener->OnMouseHover(RayOrigin, RayDir);
}

void UEditorEngine::ProcessMouseUp()
{
    for (auto* Listener : InputListeners)
        Listener->OnMouseUp();
}

bool UEditorEngine::ProcessKeyDown(const FKey& Key)
{
    for (auto* Listener : InputListeners)
    {
        if (Listener->OnKeyDown(Key))
            return true; // 기즈모가 입력을 소모함
    }
    return false;
}

// 1회 클릭 시 Actor를 선택하고, 연속 클릭 시 Component를 선택한다.
void UEditorEngine::UpdateSelection(UPrimitiveComponent* HitComp, bool bMultiSelect)
{
    if (HitComp == nullptr)
    {
        if (!bMultiSelect)
        {
            Selection->Clear();
        }
        return;
    }

    AActor* HitActor = Cast<AActor>(HitComp->GetOwner());
    if (HitActor == nullptr)
    {
        // 주인이 없는 단일 컴포넌트 처리
        if (bMultiSelect)
        {
            if (Selection->IsSelected(HitComp))
                Selection->RemoveObject(HitComp);
            else
                Selection->AddObject(HitComp);
        }
        else
        {
            Selection->Clear();
            Selection->AddObject(HitComp);
        }
        return;
    }

    if (bMultiSelect)
    {
        // 멀티 셀렉트 모드: 이미 선택되어 있다면 제거, 아니면 추가
        if (Selection->IsSelected(HitActor))
        {
            Selection->RemoveObject(HitActor);
        }
        else if (Selection->IsSelected(HitComp))
        {
            Selection->RemoveObject(HitComp);
        }
        else
        {
            Selection->AddObject(HitActor);
        }
    }
    else
    {
        // 단일 셀렉트 모드 (기존 로직: 1회 클릭 시 Actor, 연속 클릭 시 Component)
        bool bIsActorAlreadySelected = Selection->IsSelected(HitActor);
        Selection->Clear();

        if (!bIsActorAlreadySelected)
        {
            Selection->AddObject(HitActor);
        }
        else
        {
            Selection->AddObject(HitComp);
        }
    }
}

void USelection::Clear()
{
    for (auto obj : SelectedObjects)
    {
        // 1. 단일 컴포넌트가 선택되어 있었던 경우
        UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(obj);
        if (PrimComp)
        {
            PrimComp->SetSelectEffect(false);
        }

        // 2. 액터가 통째로 선택되어 있었던 경우
        AActor* Actor = Cast<AActor>(obj);
        if (Actor)
        {
            // 액터에 속한 모든 컴포넌트를 순회하며 색상 초기화
            for (auto Comp : Actor->GetOwnedComponents())
            {
                UPrimitiveComponent* ChildPrimComp = Cast<UPrimitiveComponent>(Comp);
                if (ChildPrimComp)
                {
                    ChildPrimComp->SetSelectEffect(false);
                }
            }
        }
    }
    SelectedObjects.clear();
}

void USelection::AddObject(UObject* InObject)
{
    // 이미 선택된 객체라면 중복 추가 방지 및 렌더링 오버헤드 최소화
    if (!InObject || IsSelected(InObject))
    {
        return;
    }

    SelectedObjects.push_back(InObject);

    // 1. 추가된 객체가 단일 컴포넌트인 경우
    UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(InObject);
    if (PrimComp)
    {
        PrimComp->SetSelectEffect(true);
    }

    // 2. 추가된 객체가 액터인 경우
    AActor* Actor = Cast<AActor>(InObject);
    if (Actor)
    {
        // 액터에 속한 모든 컴포넌트를 순회하며 색상 변경
        for (auto Comp : Actor->GetOwnedComponents())
        {
            UPrimitiveComponent* ChildPrimComp = Cast<UPrimitiveComponent>(Comp);
            if (ChildPrimComp)
            {
                ChildPrimComp->SetSelectEffect(true);
            }
        }
    }
}

// 이전에 무엇이 선택되어 있었는지 기억하고, 클릭 시 Actor 선택, 연속 클릭 시 Component 선택으로 분기한다.
bool USelection::IsSelected(UObject* InObject) const
{
    bool IsSelected = std::find(SelectedObjects.begin(), SelectedObjects.end(), InObject) != SelectedObjects.end();
    return IsSelected;
}

void USelection::RemoveObject(UObject* InObject)
{
    auto it = std::find(SelectedObjects.begin(), SelectedObjects.end(), InObject);
    if (it != SelectedObjects.end())
    {
        // 제거 전 선택 효과 해제
        UObject* obj = *it;
        UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(obj);
        if (PrimComp)
        {
            PrimComp->SetSelectEffect(false);
        }

        AActor* Actor = Cast<AActor>(obj);
        if (Actor)
        {
            for (auto Comp : Actor->GetOwnedComponents())
            {
                UPrimitiveComponent* ChildPrimComp = Cast<UPrimitiveComponent>(Comp);
                if (ChildPrimComp)
                {
                    ChildPrimComp->SetSelectEffect(false);
                }
            }
        }

        SelectedObjects.erase(it);
    }
}

UObject* USelection::GetSelectedObject(int32 Index) const
{
    if (SelectedObjects.size() > Index)
    {
        return SelectedObjects[Index];
    }
    return nullptr;
}

bool USelection::IsEmpty() const
{
    return SelectedObjects.empty();
}

UObject* USelection::operator[](uint32 index)
{
    return SelectedObjects[index];
}
