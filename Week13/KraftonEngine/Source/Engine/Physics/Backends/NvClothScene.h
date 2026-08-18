#pragma once

#include "Core/CoreTypes.h"

namespace nv
{
	namespace cloth
	{
		class Solver;
	}
}

class FClothInstance;

class FNvClothScene
{
public:
    bool Initialize();
    void Release();

    bool AddInstance(FClothInstance* Instance);
    void RemoveInstance(FClothInstance* Instance);

    void Simulate(float DeltaTime);

    nv::cloth::Solver* GetSolver() const { return Solver; }

private:
    nv::cloth::Solver* Solver = nullptr;
    TArray<FClothInstance*> Instances;
};
