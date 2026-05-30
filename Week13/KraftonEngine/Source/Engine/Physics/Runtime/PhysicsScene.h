#pragma once

#include "Physics/IPhysicsScene.h"

#include <algorithm>

/**
 * @file PhysicsScene.h
 * @brief Runtime Physics Scene 공통 상태를 담는 베이스 클래스.
 *
 * week13new의 FPhysicsScene 역할을 엔진의 Native / PhysX 백엔드가
 * 공통으로 사용할 수 있도록 정리한 공용 기반이다.
 */
class FPhysicsScene : public IPhysicsScene
{
public:
    virtual ~FPhysicsScene() override = default;

    EPhysicsSceneType GetSceneType() const { return SceneType; }
    void SetSceneType(EPhysicsSceneType InSceneType) { SceneType = InSceneType; }

    void* GetNativeSceneHandle() const { return NativeSceneHandle; }
    void SetNativeSceneHandle(void* InNativeSceneHandle) { NativeSceneHandle = InNativeSceneHandle; }

    const TArray<FPhysicsBodyInstance*>& GetBodyInstances() const { return RuntimeBodyInstances; }
    const TArray<FPhysicsConstraintInstance*>& GetConstraintInstances() const { return RuntimeConstraintInstances; }

    void SetEventCallback(IPhysicsEventCallback* InCallback) override { SceneEventCallback = InCallback; }
    const FPhysicsRuntimeStats& GetStats() const override { return RuntimeStats; }

protected:
    IPhysicsEventCallback* GetEventCallback() const { return SceneEventCallback; }
    FPhysicsRuntimeStats& GetMutableRuntimeStats() { return RuntimeStats; }

    void RegisterBodyInstance(FPhysicsBodyInstance* BodyInstance)
    {
        if (BodyInstance)
            RuntimeBodyInstances.push_back(BodyInstance);
    }

    void UnregisterBodyInstance(FPhysicsBodyInstance* BodyInstance)
    {
        auto It = std::find(RuntimeBodyInstances.begin(), RuntimeBodyInstances.end(), BodyInstance);
        if (It != RuntimeBodyInstances.end())
            RuntimeBodyInstances.erase(It);
    }

    void RegisterConstraintInstance(FPhysicsConstraintInstance* ConstraintInstance)
    {
        if (ConstraintInstance)
            RuntimeConstraintInstances.push_back(ConstraintInstance);
    }

    void UnregisterConstraintInstance(FPhysicsConstraintInstance* ConstraintInstance)
    {
        auto It = std::find(RuntimeConstraintInstances.begin(), RuntimeConstraintInstances.end(), ConstraintInstance);
        if (It != RuntimeConstraintInstances.end())
            RuntimeConstraintInstances.erase(It);
    }

    void ClearRegisteredInstances()
    {
        RuntimeBodyInstances.clear();
        RuntimeConstraintInstances.clear();
    }

private:
    EPhysicsSceneType                  SceneType = EPhysicsSceneType::PST_Game;
    void*                              NativeSceneHandle = nullptr;
    TArray<FPhysicsBodyInstance*>      RuntimeBodyInstances;
    TArray<FPhysicsConstraintInstance*> RuntimeConstraintInstances;
    IPhysicsEventCallback*             SceneEventCallback = nullptr;
    FPhysicsRuntimeStats               RuntimeStats;
};
