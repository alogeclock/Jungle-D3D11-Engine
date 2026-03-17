#include "World.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"
#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"

struct FHitResult;

UWorld::UWorld() {
    CreateNewLevel();
}

ULevel *UWorld::CreateNewLevel()
{
    ULevel *NewLevel = new ULevel();

    NewLevel->SetOuter(this);

    Levels.insert(NewLevel);

	CurrentLevel = NewLevel;

	return NewLevel;
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
