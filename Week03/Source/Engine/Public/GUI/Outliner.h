#pragma once
#include "Source/Engine/Object/Public/Actor.h"


class FOutliner
{
public:
    void ShowOutliner();
    void ShowObjectInfo(UObject* InObject);
    void ShowObjectProperty(UObject* InObject);
    void ShowOutliner(TArray<UObject*>& ObjectArray);
    void ShowOutliner(UObject* Object, TMap<UObject*, TArray<UObject*>>& Dependencies, TSet<UObject*>& Visited);

private:
    float outlinerHeight = 300.0f;
    float splitterThickness = 6.0f;
};