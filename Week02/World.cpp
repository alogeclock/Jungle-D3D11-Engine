#include "World.h"
#include "Object/Actor.h"

// 모든 Primitive Component 헤더 포함
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SphereComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/TriangleComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PlaneComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeArrowComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/RingComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"
#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <filesystem>

using json = nlohmann::json;

struct FHitResult;

UWorld::UWorld(const FString &InString) : UObject(InString) { CurrentLevel = CreateNewLevel("PersistentLevel"); }

UWorld::~UWorld()
{
    if (CurrentLevel != nullptr)
    {
        delete CurrentLevel;
        CurrentLevel = nullptr;
    }
}

ULevel *UWorld::CreateNewLevel(const FString &NewLevelName)
{
    ULevel *NewLevel = new ULevel(NewLevelName);

    NewLevel->SetOuter(this);

    Levels.insert(NewLevel);

    return NewLevel;
}

bool UWorld::SaveLevel(const FString &FilePath)
{
    if (CurrentLevel == nullptr)
        return false;

    std::filesystem::path path(FilePath);
    FString               CurrentSceneName = path.stem().string();

    json j;
    j["Version"] = 1;
    j["SceneName"] = CurrentSceneName;

    json primitives;
    int  currentUuid = 0;

    // 소수점 6자리까지 반올림하는 람다 함수
    auto Round6 = [](float val) -> float { return std::round(val * 1000000.0f) / 1000000.0f; };

    // 현재 레벨의 모든 액터를 순회하며 저장
    for (AActor *Actor : CurrentLevel->GetActors())
    {
        std::string primitiveType = "None";

        // 추가된 모든 Primitive Component를 식별합니다.
        for (UActorComponent *Component : Actor->GetOwnedComponents())
        {
            if (Cast<USphereComponent>(Component))
            {
                primitiveType = "Sphere";
                break;
            }
            if (Cast<UCubeComponent>(Component))
            {
                primitiveType = "Cube";
                break;
            }
            if (Cast<UTriangleComponent>(Component))
            {
                primitiveType = "Triangle";
                break;
            }
            if (Cast<UPlaneComponent>(Component))
            {
                primitiveType = "Plane";
                break;
            }
        }

        FTransform Transform = Actor->GetTransform();

        json primitiveJson;
        primitiveJson["Type"] = primitiveType;
        primitiveJson["Location"] = {Round6(Transform.Location.X), Round6(Transform.Location.Y), Round6(Transform.Location.Z)};
        primitiveJson["Rotation"] = {Round6(Transform.Rotation.X), Round6(Transform.Rotation.Y), Round6(Transform.Rotation.Z)};
        primitiveJson["Scale"] = {Round6(Transform.Scale.X), Round6(Transform.Scale.Y), Round6(Transform.Scale.Z)};

        primitives[std::to_string(currentUuid)] = primitiveJson;
        currentUuid++;
    }

    j["UUID"] = currentUuid;
    j["Primitives"] = primitives;

    std::ofstream file(FilePath);
    if (file.is_open())
    {
        file << j.dump(2);
        file.close();
        return true;
    }

    return false;
}

bool UWorld::LoadLevel(const FString &FilePath)
{
    std::ifstream file(FilePath);
    if (!file.is_open())
        return false;

    json j;
    file >> j;
    file.close();

    FString CurrentSceneName;

    if (j.contains("SceneName"))
    {
        CurrentSceneName = j["SceneName"].get<std::string>();
    }
    else
    {
        std::filesystem::path path(FilePath);
        CurrentSceneName = path.stem().string();
    }

    // 기존 레벨 메모리 해제 및 새 레벨 할당
    if (CurrentLevel != nullptr)
    {
        CurrentLevel->ClearActors();
        delete CurrentLevel;
    }

    // 여기서부터
    CurrentLevel = CreateNewLevel(CurrentSceneName);

    if (j.contains("Primitives"))
    {
        json primitives = j["Primitives"];

        for (auto &item : primitives.items())
        {
            json        primData = item.value();
            std::string type = primData["Type"].get<std::string>();

            AActor          *NewActor = GWorld->SpawnActor<AActor>();
            USceneComponent *Root = NewActor->CreateDefaultSubobject<USceneComponent>();
            NewActor->SetRootComponent(Root);
            Root->RegisterComponent();
            NewActor->SetOuter(CurrentLevel);

            // 트랜스폼 데이터 파싱 및 적용
            FTransform NewTransform;
            if (primData.contains("Location"))
            {
                auto loc = primData["Location"];
                NewTransform.Location = {loc[0], loc[1], loc[2]};
            }
            if (primData.contains("Rotation"))
            {
                auto rot = primData["Rotation"];
                NewTransform.Rotation = {rot[0], rot[1], rot[2]};
            }
            if (primData.contains("Scale"))
            {
                auto scale = primData["Scale"];
                NewTransform.Scale = {scale[0], scale[1], scale[2]};
            }

            NewActor->SetTransform(NewTransform);

            UObject *NewObj = nullptr;
            if (type == "Sphere")
                NewObj = FObjectFactory::ConstructObject(USphereComponent::StaticClass());
            else if (type == "Cube")
                NewObj = FObjectFactory::ConstructObject(UCubeComponent::StaticClass());
            else if (type == "Triangle")
                NewObj = FObjectFactory::ConstructObject(UTriangleComponent::StaticClass());
            else if (type == "Plane")
                NewObj = FObjectFactory::ConstructObject(UPlaneComponent::StaticClass());

            UPrimitiveComponent *Primitive = Cast<UPrimitiveComponent>(NewObj);
            if (Primitive != nullptr)
            {
                Primitive->SetOuter(NewActor);
                Primitive->RegisterComponent();
            }
        }
    }

    return true;
}

// 팩토리를 이용한 액터 동적 스폰 로직 구현
AActor *UWorld::SpawnActor(UClass *ClassToSpawn)
{
    // 1. 팩토리에게 신분증(UClass)을 주고 객체를 찍어내라고 명령합니다.
    UObject *NewObj = FObjectFactory::ConstructObject(ClassToSpawn);

    // 2. 만들어진 객체가 진짜 AActor가 맞는지 확인합니다.
    AActor *NewActor = Cast<AActor>(NewObj);

    if (NewActor != nullptr)
    {
        // 3. 월드에 액터를 등록하고 초기화합니다.
        if (CurrentLevel)
        {
            NewActor->SetOuter(CurrentLevel);
            // 주의: ULevel의 GetActors()가 const 참조를 반환한다면,
            // Level 클래스 내부에 AddActor(AActor* InActor) 함수를 따로 만들어 호출하는 것을 권장합니다.
            CurrentLevel->GetActors().push_back(NewActor);
        }
        return NewActor;
    }

    return nullptr; // 팩토리 생성이 실패하거나 AActor가 아니면 nullptr 반환
}

void UWorld::Render(URenderer &renderer)
{
    if (CurrentLevel)
    {
        for (AActor *actor : CurrentLevel->GetActors())
        {
            actor->IterateAllActorComponents(renderer);
        }
    }
}

FHitResult UWorld::PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection)
{
    FHitResult ClosestHit;

    if (CurrentLevel)
    {
        for (AActor *actor : CurrentLevel->GetActors())
        {
            for (UActorComponent *actorC : actor->GetOwnedComponents())
            {
                UPrimitiveComponent *Object = Cast<UPrimitiveComponent>(actorC);
                if (Object != nullptr)
                {
                    FHitResult Hit = Object->IntersectRay(RayOrigin, RayDirection);

                    // Ray와 가장 가까운 오브젝트 피킹
                    if (Hit.bHit && Hit.Distance < ClosestHit.Distance)
                    {
                        if (ClosestHit.HitComponent != nullptr)
                            ClosestHit.HitComponent->NotSelected();

                        ClosestHit = Hit;
                        Object->Selected();
                        UImGuiManager::Get().SetSelectedObject(ClosestHit.HitComponent);
                    }
                    else
                        Object->NotSelected();
                }
            }
        }
    }

    return ClosestHit;
}