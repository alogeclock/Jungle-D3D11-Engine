#include "World.h"
#include "Source/Engine/Object/Public/Actor.h"

#include "Source/Editor/Public/Grid.h"
#include "Source/Editor/Public/PivotTransformGizmo.h"
#include "Source/Editor/Public/Axis.h"

// 모든 Primitive Component 헤더 포함
#include "Source/Engine/Public/Classes/Components/SphereComponent.h"
#include "Source/Engine/Public/Classes/Components/CubeComponent.h"
#include "Source/Engine/Public/Classes/Components/TriangleComponent.h"
#include "Source/Engine/Public/Classes/Components/PlaneComponent.h"
#include "Source/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Source/Engine/Public/Classes/Components/CubeArrowComponent.h"
#include "Source/Engine/Public/Classes/Components/RingComponent.h"
#include "Source/Engine/Public/Classes/Components/AxisComponent.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <filesystem>

using json = nlohmann::json;

struct FHitResult;

UWorld::UWorld(const FString &InString) : UObject(InString) 
{
    CurrentLevel = CreateNewLevel("PersistentLevel");
    LineBatcherComponent = new ULineBatcherComponent("LineBatcherComponent");
}

UWorld::~UWorld()
{
    if (LineBatcherComponent)
    {
        delete LineBatcherComponent;
        LineBatcherComponent = nullptr;
    }

    for (ULevel* Level : Levels)
    {
        if (Level != nullptr)
        {
            delete Level;
        }
    }
    Levels.clear();

    CurrentLevel = nullptr;
}

ULevel *UWorld::CreateNewLevel(const FString &NewLevelName)
{
    ULevel *NewLevel = new ULevel(NewLevelName);
    NewLevel->SetOuter(this);
    Levels.insert(NewLevel);

    SpawnActorForLevel<AGrid>(NewLevel, "EditorGrid");
    SpawnActorForLevel<AAxis>(NewLevel, "EditorAxis");
    SpawnActorForLevel<APivotTransformGizmo>(NewLevel, "EditorGizmo");

    return NewLevel;
}

bool UWorld::SaveLevel(const std::wstring &FilePath)
{
    if (CurrentLevel == nullptr)
        return false;

    std::filesystem::path path(FilePath);

    // [추가됨] wstring을 UTF-8 인코딩의 string으로 안전하게 변환하는 람다 함수
    auto WStringToUTF8 = [](const std::wstring& wstr) -> std::string {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    };

    FString CurrentSceneName = WStringToUTF8(path.stem().wstring());

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

    std::ofstream file(path);
    if (file.is_open())
    {
        file << j.dump(2);
        file.close();
        return true;
    }

    return false;
}

bool UWorld::LoadLevel(const std::wstring &FilePath)
{
    std::filesystem::path path = std::filesystem::path(FilePath);

    std::ifstream file(path);
    if (!file.is_open())
        return false;

    json j;

    try
    {
        file >> j; // JSON 파싱 시도
    }
    catch (const json::parse_error&)
    {
        // JSON 형식이 잘못되었거나 파일이 비어있는 경우
        file.close();
        return false;
    }
    catch (const std::exception&)
    {
        // 기타 표준 예외 발생 시
        file.close();
        return false;
    }

    file.close();

    FString CurrentSceneName;
    ULevel *OldLevel = CurrentLevel;

    // 1. contains()와 함께 is_string()을 추가로 확인하여 타입 불일치 오류 방지
    try
    {
        if (j.contains("SceneName") && j["SceneName"].is_string())
        {
            CurrentSceneName = j["SceneName"].get<std::string>();
        }
        else
        {
            // 2. else 블록으로 넘어왔을 때 한자/한글 경로로 프로그램이 종료되는 현상 차단
            std::wstring wStem = path.stem().wstring();
            int          size_needed = WideCharToMultiByte(CP_UTF8, 0, wStem.c_str(), -1, NULL, 0, NULL, NULL);
            std::string  utf8Stem(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, wStem.c_str(), -1, &utf8Stem[0], size_needed, NULL, NULL);

            CurrentSceneName = utf8Stem;
        }
        CurrentLevel = CreateNewLevel(CurrentSceneName);

        if (j.contains("Primitives"))
        {
            json primitives = j["Primitives"];

            for (auto &item : primitives.items())
            {
                json primData = item.value();

                if (!primData.contains("Type") || !primData["Type"].is_string())
                {
                    throw std::runtime_error("Invalid or missing 'Type' format.");
                }

                std::string type = primData["Type"].get<std::string>();

                AActor *NewActor = GWorld->SpawnActor<AActor>();
                if (NewActor == nullptr)
                    throw std::runtime_error("Failed to spawn actor.");

                USceneComponent *Root = NewActor->CreateDefaultSubobject<USceneComponent>();
                if (Root != nullptr)
                {
                    NewActor->SetRootComponent(Root);
                    Root->RegisterComponent();
                    NewActor->SetOuter(CurrentLevel);
                }

                // 트랜스폼 데이터 파싱 및 적용
                FTransform NewTransform;
                if (primData.contains("Location") && primData["Location"].is_array() && primData["Location"].size() >= 3 &&
                    primData["Location"][0].is_number() && primData["Location"][1].is_number() && primData["Location"][2].is_number())
                {
                    auto &loc = primData["Location"];
                    NewTransform.Location = {loc[0].get<float>(), loc[1].get<float>(), loc[2].get<float>()};
                }
                else
                    throw std::runtime_error("Invalid Location format.");

                if (primData.contains("Rotation") && primData["Rotation"].is_array() && primData["Rotation"].size() >= 3 &&
                    primData["Rotation"][0].is_number() && primData["Rotation"][1].is_number() && primData["Rotation"][2].is_number())
                {
                    auto &rot = primData["Rotation"];
                    NewTransform.Rotation = {rot[0].get<float>(), rot[1].get<float>(), rot[2].get<float>()};
                }
                else
                    throw std::runtime_error("Invalid Rotation format.");

                if (primData.contains("Scale") && primData["Scale"].is_array() && primData["Scale"].size() >= 3 && primData["Scale"][0].is_number() &&
                    primData["Scale"][1].is_number() && primData["Scale"][2].is_number())
                {
                    auto &scale = primData["Scale"];
                    NewTransform.Scale = {scale[0].get<float>(), scale[1].get<float>(), scale[2].get<float>()};
                }
                else
                    throw std::runtime_error("Invalid Scale format.");

                NewActor->SetTransform(NewTransform);

                UObject *NewObj = nullptr;
                if (type == "None")
                    continue;
                else if (type == "Sphere")
                    NewObj = FObjectFactory::ConstructObject(USphereComponent::StaticClass());
                else if (type == "Cube")
                    NewObj = FObjectFactory::ConstructObject(UCubeComponent::StaticClass());
                else if (type == "Triangle")
                    NewObj = FObjectFactory::ConstructObject(UTriangleComponent::StaticClass());
                else if (type == "Plane")
                    NewObj = FObjectFactory::ConstructObject(UPlaneComponent::StaticClass());
                else
                    throw std::runtime_error("Unknown Primitive Type."); // 알 수 없는 타입 시 중단

                if (NewObj != nullptr)
                {
                    UPrimitiveComponent *Primitive = Cast<UPrimitiveComponent>(NewObj);
                    if (Primitive != nullptr)
                    {
                        Primitive->SetOuter(NewActor);
                        Primitive->RegisterComponent();
                    }
                    else
                    {
                        delete NewObj;
                        throw std::runtime_error("Failed to cast PrimitiveComponent.");
                    }
                }
            }
        }

        // 모든 과정이 완벽하게 성공했을 때 기존 레벨을 삭제합니다.
        if (OldLevel != nullptr)
        {
            Levels.erase(OldLevel); 
            delete OldLevel;
        }
    }
    catch (const std::exception&)
    {
        // 1. 만들다 만 잘못된 씬(현재 레벨)을 월드 목록에서 지우고 메모리를 해제합니다.
        Levels.erase(CurrentLevel);
        delete CurrentLevel;

        // 2. 아까 백업해두었던 기존 씬(OldLevel)을 다시 현재 씬으로 복구합니다.
        CurrentLevel = OldLevel;

        return false;
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

    LineBatcherComponent->Render(renderer);
    LineBatcherComponent->Flush();
}

void UWorld::Tick(float deltaTime)
{
    if (CurrentLevel)
    {
        for (AActor *actor : CurrentLevel->GetActors())
        {
            actor->Tick(deltaTime);
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