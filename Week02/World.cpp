#include "World.h"
#include "Object/Actor.h"
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
