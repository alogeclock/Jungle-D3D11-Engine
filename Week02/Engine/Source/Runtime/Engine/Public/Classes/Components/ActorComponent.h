#pragma once

#include "Object/Object.h"

class AActor;

// 가장 기본이 되는 컴포넌트
class UActorComponent : public UObject
{
  public:
    UActorComponent();
    virtual ~UActorComponent();

    // 1. [UE 기본] 자신을 액터와 시스템에 등록하는 함수
    void RegisterComponent();

    // 2. [UE 기본] 등록이 완료된 후, 초기 세팅을 진행하는 함수
    virtual void InitializeComponent();

    // 3. [이름 변경] UE에서는 매 프레임 갱신을 Update가 아닌 'TickComponent'로 부릅니다.
    virtual void TickComponent(float DeltaTime);

    // 4. [핵심 추가] 실제 UE에서 컴포넌트가 자기 주인을 찾을 때 쓰는 국민 함수
    AActor *GetOwner() const;
};