#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ActorComponent.h"

#include "Object/Actor.h"

UActorComponent::UActorComponent() {}

UActorComponent::~UActorComponent() {}

void UActorComponent::RegisterComponent()
{
    // 1. UE의 방식대로 내 주인(AActor)을 가져옵니다.
    AActor *MyOwner = GetOwner();

    // 2. 주인이 존재한다면, 주인이 열어둔 전용 문으로 나 자신(this)을 밀어 넣습니다.
    if (MyOwner != nullptr)
    {
        MyOwner->AddOwnedComponent(this);
    }

    // 3. 물리 엔진이나 렌더러에 등록하는 코드가 추후 여기에 들어갑니다.

    // 4. 시스템 등록이 무사히 끝났으므로 초기화 함수를 호출합니다.
    InitializeComponent();
}

void UActorComponent::InitializeComponent() {}

// Update 대신 UE의 명칭인 TickComponent로 구현합니다.
void UActorComponent::TickComponent(float DeltaTime) {}

// [추가] 나를 감싸고 있는 주인(Outer)을 AActor로 변환해서 돌려줍니다.
AActor *UActorComponent::GetOwner() const
{
    // 내 Outer를 가져와서 AActor 타입인지 확인(dynamic_cast) 후 반환합니다.
    // 만약 주인이 액터가 아니라 다른 컴포넌트거나 월드라면 nullptr을 반환하여 안전성을 보장합니다.
    return Cast<AActor>(GetOuter());
}
