#pragma once

#include "Physics/Systems/Cloth/ClothSimulationTypes.h"
#include "Physics/Systems/Cloth/ClothCollisionTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Math/Quat.h"

#include <foundation/PxVec4.h>
#include <foundation/PxVec3.h>
#include <foundation/PxQuat.h>

namespace nv
{
	namespace cloth
	{
		class Fabric;
		class Cloth;
	}
}

class FClothInstance
{
public:
    FClothInstance() = default;
    ~FClothInstance() { Release(); }

    FClothInstance(const FClothInstance&) = delete;
    FClothInstance& operator=(const FClothInstance&) = delete;

    bool InitializeGrid(const FClothBuildDesc& BuildDesc);
    bool InitializeMesh(const FClothBuildDesc& BuildDesc, const FMeshDataView& MeshView);
    void Release();

    void ApplySimulationSettings(const FClothSimulationSettings& Settings, const FClothConstraintSettings& ConstraintSettings);
    void UpdateCollision(const FClothCollisionData& CollisionData);
    void SetSimulationSpaceTransform(const FVector& WorldLocation, const FQuat& WorldRotation, bool bTeleport);

    // Called once by FNvClothScene after the Solver step.
    void PostSceneSimulate();

    nv::cloth::Cloth* GetCloth() const { return Cloth; }
    bool IsInitialized() const { return Cloth != nullptr; }

    const TArray<FVertexPNCTT>& GetRenderVertices() const { return RenderVertices; }
    const TArray<uint32>& GetRenderIndices() const { return Triangles; }
    uint64 GetRenderRevision() const { return RenderRevision; }
    int32 GetSubstepCount() const { return SimulationSettings.SubstepCount; }
    int32 GetParticleCount() const { return static_cast<int32>(Particles.size()); }
    int32 GetConstraintCount() const { return static_cast<int32>(RestValues.size()); }
    int32 GetColliderCount() const { return LastColliderCount; }
    int32 GetCollisionTestCount() const { return LastCollisionTestCount; }
    bool IsSelfCollisionEnabled() const { return SimulationSettings.bEnableSelfCollision; }

    bool GetLocalBounds(FVector& OutCenter, FVector& OutExtent) const;

private:
    void ClearBuildData();
    bool CreateNvClothObjects(const FClothBuildDesc& BuildDesc);
    void UpdateRenderVerticesFromParticles();

    void BuildGridParticles(const FClothBuildDesc& BuildDesc);
    void BuildGridConstraints(const FClothBuildDesc& BuildDesc);
    void BuildGridTriangles(const FClothGridDesc& GridDesc);

private:
    nv::cloth::Fabric* Fabric = nullptr;
    nv::cloth::Cloth* Cloth = nullptr;

    TArray<physx::PxVec4> Particles;

    TArray<uint32> PhaseIndices;
    TArray<uint32> Sets;
    TArray<float> RestValues;
    TArray<float> StiffnessValues;
    TArray<uint32> Indices;
    TArray<uint32> Anchors;
    TArray<float> TetherLengths;
    TArray<uint32> Triangles;

    TArray<FVertexPNCTT> RenderVertices;
    TArray<FVector2> SourceUVs;
    TArray<FVector4> SourceTangents;

    FClothSimulationSettings SimulationSettings;
    FClothConstraintSettings ConstraintSettings;

    uint64 RenderRevision = 0;
    int32 GridWidth = 0;
    int32 GridHeight = 0;
    int32 LastColliderCount = 0;
    int32 LastCollisionTestCount = 0;
    bool bUseSourceMeshAttributes = false;
};
