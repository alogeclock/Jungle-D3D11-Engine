#include "World.h"
#include "Object/Actor.h"

// 모든 Primitive Component 헤더 포함
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SphereComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/TriangleComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PlaneComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/TorusComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeArrowComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/RingComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct FHitResult;

UWorld::UWorld() {
    CurrentLevel = new ULevel();
    CurrentLevel->SetOuter(this);
}

UWorld::~UWorld()
{
    if (CurrentLevel != nullptr)
    {
        delete CurrentLevel;
        CurrentLevel = nullptr;
    }
}

bool UWorld::SaveLevel(const std::string& FilePath)
{
    if (CurrentLevel == nullptr)
        return false;

    json j;
    j["Version"] = 1;
    
    json primitives;
    int currentUuid = 0;

    // 현재 레벨의 모든 액터를 순회하며 저장
    for (AActor* Actor : CurrentLevel->GetActors())
    {
        std::string primitiveType = "None";
        
        // 추가된 모든 Primitive Component를 식별합니다.
        for (UActorComponent* Component : Actor->GetOwnedComponents())
        {
            if (Cast<USphereComponent>(Component))        { primitiveType = "Sphere"; break; }
            if (Cast<UCubeComponent>(Component))          { primitiveType = "Cube"; break; }
            if (Cast<UTriangleComponent>(Component))      { primitiveType = "Triangle"; break; }
            if (Cast<UPlaneComponent>(Component))         { primitiveType = "Plane"; break; }
            if (Cast<UTorusComponent>(Component))         { primitiveType = "Torus"; break; }
            if (Cast<UArrowComponent>(Component))         { primitiveType = "Arrow"; break; }
            if (Cast<UCubeArrowComponent>(Component))     { primitiveType = "CubeArrow"; break; }
            if (Cast<URingComponent>(Component))          { primitiveType = "Ring"; break; }
            if (Cast<UAxisComponent>(Component))          { primitiveType = "Axis"; break; }
        }

        FTransform Transform = Actor->GetTransform();
        
        json primitiveJson;
        primitiveJson["Type"] = primitiveType;
        primitiveJson["Location"] = { Transform.Location.X, Transform.Location.Y, Transform.Location.Z };
        primitiveJson["Rotation"] = { Transform.Rotation.X, Transform.Rotation.Y, Transform.Rotation.Z }; 
        primitiveJson["Scale"]    = { Transform.Scale.X, Transform.Scale.Y, Transform.Scale.Z };

        primitives[std::to_string(currentUuid)] = primitiveJson;
        currentUuid++;
    }

    j["NextUUID"] = currentUuid;
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

bool UWorld::LoadLevel(const std::string& FilePath)
{
    std::ifstream file(FilePath);
    if (!file.is_open())
        return false;

    json j;
    file >> j;
    file.close();

    // 기존 레벨 메모리 해제 및 새 레벨 할당
    if (CurrentLevel != nullptr)
    {
        // TODO: 레벨 내부의 Actor들 메모리 해제 로직이 추가되어야 합니다.
        CurrentLevel->GetActors().clear(); 
        delete CurrentLevel;
    }
    
    CurrentLevel = new ULevel();
    CurrentLevel->SetOuter(this);

    if (j.contains("Primitives"))
    {
        json primitives = j["Primitives"];
        
        for (auto& item : primitives.items())
        {
            json primData = item.value();
            std::string type = primData["Type"].get<std::string>();
            
            // 액터 생성
            AActor* NewActor = new AActor(); 
            NewActor->SetOuter(CurrentLevel);

            // 문자열 기반으로 매칭되는 타입별 컴포넌트 생성
            UPrimitiveComponent* PrimitiveComp = nullptr;
            if (type == "Sphere")             PrimitiveComp = new USphereComponent();
            else if (type == "Cube")          PrimitiveComp = new UCubeComponent();
            else if (type == "Triangle")      PrimitiveComp = new UTriangleComponent();
            else if (type == "Plane")         PrimitiveComp = new UPlaneComponent();
            else if (type == "Torus")         PrimitiveComp = new UTorusComponent();
            else if (type == "Arrow")         PrimitiveComp = new UArrowComponent();
            else if (type == "CubeArrow")     PrimitiveComp = new UCubeArrowComponent();
            else if (type == "Ring")          PrimitiveComp = new URingComponent();
            else if (type == "Axis")          PrimitiveComp = new UAxisComponent();

            if (PrimitiveComp != nullptr)
            {
                PrimitiveComp->SetOuter(NewActor);
                NewActor->SetRootComponent(PrimitiveComp);
                NewActor->AddOwnedComponent(PrimitiveComp);
            }

            // 트랜스폼 데이터 파싱 및 적용
            FTransform NewTransform;
            if (primData.contains("Location"))
            {
                auto loc = primData["Location"];
                NewTransform.Location = { loc[0], loc[1], loc[2] }; 
            }
            if (primData.contains("Rotation"))
            {
                auto rot = primData["Rotation"];
                NewTransform.Rotation = { rot[0], rot[1], rot[2] }; 
            }
            if (primData.contains("Scale"))
            {
                auto scale = primData["Scale"];
                NewTransform.Scale = { scale[0], scale[1], scale[2] }; 
            }
            
            NewActor->SetTransform(NewTransform);

            // 레벨에 액터 추가
            CurrentLevel->GetActors().push_back(NewActor); 
        }
    }

    return true;
}

void UWorld::Render(URenderer &renderer)
{
    if (CurrentLevel)
    {
        for (AActor* actor : CurrentLevel->GetActors())
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
                        if(ClosestHit.HitComponent != nullptr)
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