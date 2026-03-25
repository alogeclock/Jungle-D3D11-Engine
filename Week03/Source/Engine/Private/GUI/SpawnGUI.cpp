#include "Source/Engine/Public/GUI/SpawnGUI.h"
#include "Source/Editor/Public/Application.h"
#include "Source/Engine/Public/Classes/Components/TextComponent.h"
#include "Source/Engine/Public/Classes/Components/UUIDTextComponent.h"
#include "Source/Engine/Public/Classes/Components/ParticleSubUVComponent.h"
#include "World.h"

bool FSpawnGUI::GetVisible()
{
    return bIsVisible;
}

void FSpawnGUI::SetVisible(bool InVisible)
{
    bIsVisible = InVisible;
}

bool FSpawnGUI::IsInBound(ImVec2 InScreenMousePosition)
{
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    ImVec2 min = windowPos;
    ImVec2 max = ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y);

    return (InScreenMousePosition.x >= min.x && InScreenMousePosition.x <= max.x && InScreenMousePosition.y >= min.y &&
            InScreenMousePosition.y <= max.y);
}

void FSpawnGUI::ShowDetail()
{
    const char* PrimitiveTypeStrings[] = {"Sphere", "Cube", "Triangle", "Plane", "Text", "UI", "Explosion"};

    static int Primitive = 0;

    ImGui::Combo("Primitive", &Primitive, PrimitiveTypeStrings, IM_ARRAYSIZE(PrimitiveTypeStrings));

    if (ImGui::Button("Spawn"))
    {
        UClass* ComponentClassToSpawn = nullptr;

        switch (Primitive)
        {
        case 0:
            ComponentClassToSpawn = USphereComponent::StaticClass();
            break;
        case 1:
            ComponentClassToSpawn = UCubeComponent::StaticClass();
            break;
        case 2:
            ComponentClassToSpawn = UTriangleComponent::StaticClass();
            break;
        case 3:
            ComponentClassToSpawn = UPlaneComponent::StaticClass();
            break;
        case 4:
            ComponentClassToSpawn = UTextComponent::StaticClass();
            break;
        case 5:
            ComponentClassToSpawn = UUUIDTextComponent::StaticClass();
            break;
        case 6:
            ComponentClassToSpawn = UParticleSubUVComponent::StaticClass();
            break;
        }

        SpawnPrimitiveComponent(ComponentClassToSpawn, SpawnCount);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.3f);
    ImGui::DragInt("Number of spawn", &SpawnCount, 0.05f, 1, 10);
}

void FSpawnGUI::DrawContextMenu()
{
    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragInt("Count", &SpawnCount, 0.1f, 1, 100);

    if (ImGui::MenuItem("Sphere"))
    {
        SpawnPrimitiveComponent(USphereComponent::StaticClass(), SpawnCount);
    }
    if (ImGui::MenuItem("Cube"))
    {
        SpawnPrimitiveComponent(UCubeComponent::StaticClass(), SpawnCount);
    }
    if (ImGui::MenuItem("Triangle"))
    {
        SpawnPrimitiveComponent(UTriangleComponent::StaticClass(), SpawnCount);
    }
    if (ImGui::MenuItem("Plane"))
    {
        SpawnPrimitiveComponent(UPlaneComponent::StaticClass(), SpawnCount);
    }
    if (ImGui::MenuItem("Text"))
    {
        SpawnPrimitiveComponent(UTextComponent::StaticClass(), SpawnCount);
    }
    if (ImGui::MenuItem("Smoke"))
    {
        SpawnPrimitiveComponent(UParticleSubUVComponent::StaticClass(), SpawnCount);
    }
}

void FSpawnGUI::SpawnPrimitiveComponent(UClass* ComponentClassToSpawn, int NumberOfSpawn)
{
    if (ComponentClassToSpawn == nullptr)
    {
        return;
    }

    for (int i = 0; i < NumberOfSpawn; i++)
    {
        AActor* NewActor = GWorld->SpawnActor<AActor>();
        USceneComponent* Root = NewActor->CreateDefaultSubobject<USceneComponent>();
        NewActor->SetRootComponent(Root);
        Root->RegisterComponent();

        UObject* NewComponent = FObjectFactory::ConstructObject(ComponentClassToSpawn);
        UPrimitiveComponent* DynamicPrimitive = Cast<UPrimitiveComponent>(NewComponent);
        if (DynamicPrimitive == nullptr)
        {
            continue;
        }

        DynamicPrimitive->SetOuter(NewActor);
        DynamicPrimitive->RegisterComponent();

        if (UTextComponent* TextComp = Cast<UTextComponent>(DynamicPrimitive))
        {
            if (TextComp->IsExactly(UTextComponent::StaticClass()))
            {
                TextComp->SetText(FString(reinterpret_cast<const char*>(u8"박상혁 김호준 전현길 김기홍")));
            }
        }

        UObject* NewUUUIDComponent = FObjectFactory::ConstructObject(UUUIDTextComponent::StaticClass());
        UUUIDTextComponent* UUUID = Cast<UUUIDTextComponent>(NewUUUIDComponent);
        if (UUUID == nullptr)
        {
            continue;
        }

        UUUID->SetOuter(NewActor);
        UUUID->RegisterComponent();
        UUUID->SetText(NewActor->GetUUID());
    }
}
