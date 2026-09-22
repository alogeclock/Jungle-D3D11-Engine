#pragma once

#include "Source/Engine/Object/Public/Object.h"
#include "Source/Engine/Object/Public/Actor.h"
#include "Source/Editor/Public/EditorEngine.h"
#include "Source/Engine/Public/Classes/Components/BillboardComponent.h"

class AEditorSpriteActor : public AActor
{
public:
    AEditorSpriteActor(const FString& InString);
    virtual ~AEditorSpriteActor() override;

    virtual void Tick(float DeltaTime) override;
    bool GetGizmoTargetTransform(FTransform& OutTransform);

private:
    UBillboardComponent* SpriteComponent = nullptr;
};
