#include "Physics/Backends/NvClothScene.h"

#include "Physics/Backends/NvClothSDK.h"
#include "Physics/Systems/Cloth/ClothInstance.h"
#include "Profiling/Stats.h"

#include <NvCloth/Cloth.h>
#include <NvCloth/Factory.h>
#include <NvCloth/Solver.h>

#include <algorithm>

bool FNvClothScene::Initialize()
{
    if (Solver)
    {
        return true;
    }

    if (!FNvClothSDK::Get().Initialize())
    {
        return false;
    }

    nv::cloth::Factory* Factory = FNvClothSDK::Get().GetFactory();
    if (!Factory)
    {
        return false;
    }

    Solver = Factory->createSolver();
    return Solver != nullptr;
}

void FNvClothScene::Release()
{
    if (!Solver)
    {
        Instances.clear();
        return;
    }

    for (FClothInstance* Instance : Instances)
    {
        if (Instance && Instance->GetCloth())
        {
            Solver->removeCloth(Instance->GetCloth());
        }
    }
    Instances.clear();

    delete Solver;
    Solver = nullptr;
}

bool FNvClothScene::AddInstance(FClothInstance* Instance)
{
    if (!Instance || !Instance->GetCloth())
    {
        return false;
    }

    if (!Solver && !Initialize())
    {
        return false;
    }

    if (std::find(Instances.begin(), Instances.end(), Instance) != Instances.end())
    {
        return true;
    }

    Solver->addCloth(Instance->GetCloth());
    Instances.push_back(Instance);
    return true;
}

void FNvClothScene::RemoveInstance(FClothInstance* Instance)
{
    if (!Instance)
    {
        return;
    }

    auto It = std::find(Instances.begin(), Instances.end(), Instance);
    if (It == Instances.end())
    {
        return;
    }

    if (Solver && Instance->GetCloth())
    {
        Solver->removeCloth(Instance->GetCloth());
    }
    Instances.erase(It);
}

void FNvClothScene::Simulate(float DeltaTime)
{
    SCOPE_STAT_CAT("Simulate", "Cloth");

    if (!Solver || DeltaTime <= 0.0f || Instances.empty())
    {
        return;
    }

    if (!Solver->beginSimulation(DeltaTime))
    {
        return;
    }

    const int32 ChunkCount = Solver->getSimulationChunkCount();
    for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
    {
        Solver->simulateChunk(ChunkIndex);
    }

    Solver->endSimulation();

    for (FClothInstance* Instance : Instances)
    {
        if (Instance)
        {
            Instance->PostSceneSimulate();
        }
    }
}
