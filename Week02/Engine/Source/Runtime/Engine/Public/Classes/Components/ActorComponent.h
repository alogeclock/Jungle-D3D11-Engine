#pragma once

#include "Object/Object.h"

// 가장 기본이 되는 컴포넌트
class UActorComponent : public UObject
{
  public:
    UActorComponent();
    virtual ~UActorComponent();

    virtual void InitializeComponent(); // 컴포넌트 초기화 함수
    virtual void Update(float DeltaTime);
};