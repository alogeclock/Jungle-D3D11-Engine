#include "Physics/Systems/Cloth/ClothInstance.h"
#include "Physics/Backends/NvClothSDK.h"
#include "Mesh/StaticMeshAsset.h"
#include "Core/Log.h"

#include <NvCloth/Factory.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Cloth.h>
#include <NvCloth/PhaseConfig.h>
#include <NvCloth/Range.h>

#include <foundation/PxQuat.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>

namespace
{
    physx::PxVec3 ToPxVec3(const FVector& V)
    {
        return physx::PxVec3(V.X, V.Y, V.Z);
    }

    physx::PxQuat ToPxQuat(const FQuat& Q)
    {
        return physx::PxQuat(Q.X, Q.Y, Q.Z, Q.W);
    }

    float LengthSq(const FVector& V)
    {
        return V.X * V.X + V.Y * V.Y + V.Z * V.Z;
    }

    float Distance(const physx::PxVec4& A, const physx::PxVec4& B)
    {
        const float Dx = A.x - B.x;
        const float Dy = A.y - B.y;
        const float Dz = A.z - B.z;
        return std::sqrt(Dx * Dx + Dy * Dy + Dz * Dz);
    }

    template<typename T>
    nv::cloth::Range<const T> MakeConstRange(const TArray<T>& Array)
    {
        if (Array.empty())
        {
            return nv::cloth::Range<const T>();
        }
        const T* Begin = Array.data();
        return nv::cloth::Range<const T>(Begin, Begin + Array.size());
    }
}

void FClothInstance::ClearBuildData()
{
    Particles.clear();
    PhaseIndices.clear();
    Sets.clear();
    RestValues.clear();
    StiffnessValues.clear();
    Indices.clear();
    Anchors.clear();
    TetherLengths.clear();
    Triangles.clear();
    RenderVertices.clear();
    SourceUVs.clear();
    SourceTangents.clear();
    RenderRevision = 0;
    GridWidth = 0;
    GridHeight = 0;
    bUseSourceMeshAttributes = false;
}

bool FClothInstance::InitializeGrid(const FClothBuildDesc& BuildDesc)
{
    Release();
    ClearBuildData();

    if (!BuildDesc.IsValid())
    {
        return false;
    }

    BuildGridParticles(BuildDesc);
    BuildGridConstraints(BuildDesc);
    BuildGridTriangles(BuildDesc.GridDesc);

    if (Particles.empty() || RestValues.empty() || Triangles.empty())
    {
        return false;
    }

    return CreateNvClothObjects(BuildDesc);
}

bool FClothInstance::InitializeMesh(const FClothBuildDesc& BuildDesc, const FMeshDataView& MeshView)
{
    Release();
    ClearBuildData();

    if (!MeshView.IsValid() || MeshView.VertexCount == 0 || MeshView.IndexCount < 3)
    {
        return false;
    }

    SimulationSettings = BuildDesc.SimulationSettings;
    ConstraintSettings = BuildDesc.ConstraintSettings;
    bUseSourceMeshAttributes = true;

    SourceUVs.resize(MeshView.VertexCount, FVector2(0.0f, 0.0f));
    SourceTangents.resize(MeshView.VertexCount, FVector4(1.0f, 0.0f, 0.0f, 1.0f));

    float MaxZ = -FLT_MAX;
    for (uint32 i = 0; i < MeshView.VertexCount; ++i)
    {
        const FVector& P = MeshView.GetPosition(i);
        MaxZ = (std::max)(MaxZ, P.Z);
    }

    const float PinThreshold = BuildDesc.GridDesc.Spacing;
    for (uint32 i = 0; i < MeshView.VertexCount; ++i)
    {
        const FNormalVertex& V = MeshView.GetVertex<FNormalVertex>(i);
        const bool bPinned = BuildDesc.GridDesc.PinMode != EClothPinMode::None && V.pos.Z >= MaxZ - PinThreshold;
        const float InvMass = bPinned ? 0.0f : 1.0f;

        Particles.push_back(physx::PxVec4(
            V.pos.X + BuildDesc.GridDesc.InitialOffset.X,
            V.pos.Y + BuildDesc.GridDesc.InitialOffset.Y,
            V.pos.Z + BuildDesc.GridDesc.InitialOffset.Z,
            InvMass));

        SourceUVs[i] = V.tex;
        SourceTangents[i] = V.tangent;
    }

    Triangles.reserve(MeshView.IndexCount);
    for (uint32 i = 0; i + 2 < MeshView.IndexCount; i += 3)
    {
        const uint32 I0 = MeshView.IndexData[i + 0];
        const uint32 I1 = MeshView.IndexData[i + 1];
        const uint32 I2 = MeshView.IndexData[i + 2];
        if (I0 < MeshView.VertexCount && I1 < MeshView.VertexCount && I2 < MeshView.VertexCount &&
            I0 != I1 && I1 != I2 && I2 != I0)
        {
            Triangles.push_back(I0);
            Triangles.push_back(I1);
            Triangles.push_back(I2);
        }
    }

    constexpr int32 MeshConstraintSetCount = 8;
    TArray<uint32> SetIndices[MeshConstraintSetCount];
    TArray<float> SetRestValues[MeshConstraintSetCount];
    TSet<uint64> UsedEdges;

    auto AddConstraint = [&](int32 SetIndex, uint32 A, uint32 B)
    {
        if (A == B || A >= Particles.size() || B >= Particles.size())
        {
            return;
        }

        SetIndices[SetIndex].push_back(A);
        SetIndices[SetIndex].push_back(B);
        SetRestValues[SetIndex].push_back(Distance(Particles[A], Particles[B]));
    };

    auto AddEdge = [&](uint32 A, uint32 B)
    {
        if (A > B)
        {
            std::swap(A, B);
        }

        const uint64 Key = (static_cast<uint64>(A) << 32) | static_cast<uint64>(B);
        if (!UsedEdges.insert(Key).second)
        {
            return;
        }

        const int32 SetIndex = static_cast<int32>((A + B) % MeshConstraintSetCount);
        AddConstraint(SetIndex, A, B);
    };

    for (uint32 i = 0; i + 2 < Triangles.size(); i += 3)
    {
        const uint32 I0 = Triangles[i + 0];
        const uint32 I1 = Triangles[i + 1];
        const uint32 I2 = Triangles[i + 2];
        AddEdge(I0, I1);
        AddEdge(I1, I2);
        AddEdge(I2, I0);
    }

    for (int32 SetIndex = 0; SetIndex < MeshConstraintSetCount; ++SetIndex)
    {
        Indices.insert(Indices.end(), SetIndices[SetIndex].begin(), SetIndices[SetIndex].end());
        RestValues.insert(RestValues.end(), SetRestValues[SetIndex].begin(), SetRestValues[SetIndex].end());
        Sets.push_back(static_cast<uint32>(RestValues.size()));
        PhaseIndices.push_back(static_cast<uint32>(SetIndex));
    }

    if (RestValues.empty())
    {
        return false;
    }

    return CreateNvClothObjects(BuildDesc);
}

void FClothInstance::BuildGridParticles(const FClothBuildDesc& BuildDesc)
{
    const FClothGridDesc& GridDesc = BuildDesc.GridDesc;
    GridWidth = GridDesc.Width;
    GridHeight = GridDesc.Height;
    SimulationSettings = BuildDesc.SimulationSettings;
    ConstraintSettings = BuildDesc.ConstraintSettings;

	// 천의 핀 모드에 따라 고정 여부 판정
    auto IsPinned = [&](int32 X, int32 Y)
    {
        switch (GridDesc.PinMode)
        {
        case EClothPinMode::TopRow:
            return Y == 0;
        case EClothPinMode::TopCorners:
            return Y == 0 && (X == 0 || X == GridDesc.Width - 1);
        case EClothPinMode::LeftEdge:
            return X == 0;
        case EClothPinMode::None:
        case EClothPinMode::Custom:
        default:
            return false;
        }
    };

	// Pin 제약: 입자 고정, 고정된 입자는 질량이 무한대(역질량 0)로 설정
    Particles.reserve(static_cast<size_t>(GridDesc.Width) * static_cast<size_t>(GridDesc.Height));
    for (int32 Y = 0; Y < GridDesc.Height; ++Y)
    {
        for (int32 X = 0; X < GridDesc.Width; ++X)
        {
            const float InvMass = IsPinned(X, Y) ? 0.0f : 1.0f;
            Particles.push_back(physx::PxVec4(
                X * GridDesc.Spacing + GridDesc.InitialOffset.X,
                Y * GridDesc.Spacing + GridDesc.InitialOffset.Y,
                GridDesc.InitialOffset.Z, InvMass));
        }
    }

	// Tether 제약: 늘어짐 방지, 자신과 같은 열에 있는 최상단 고정점과 직접 연결되어 최대 길이를 유지
    if (ConstraintSettings.bUseTether && GridDesc.PinMode != EClothPinMode::None)
    {
        for (int32 Y = 0; Y < GridDesc.Height; ++Y)
        {
            for (int32 X = 0; X < GridDesc.Width; ++X)
            {
                const uint32 ParticleIndex = static_cast<uint32>(Y * GridDesc.Width + X);
                const uint32 AnchorIndex = static_cast<uint32>(X); // Top row, same column.
                Anchors.push_back(AnchorIndex);
                TetherLengths.push_back(Distance(Particles[ParticleIndex], Particles[AnchorIndex]));
            }
        }
    }
}

void FClothInstance::BuildGridConstraints(const FClothBuildDesc& BuildDesc)
{
    const FClothGridDesc& GridDesc = BuildDesc.GridDesc;

    constexpr int32 ConstraintSetCount = 12;
    TArray<uint32> SetIndices[ConstraintSetCount];
    TArray<float> SetRestValues[ConstraintSetCount];

    auto AddConstraintToSet = [&](int32 SetIndex, uint32 A, uint32 B)
    {
        SetIndices[SetIndex].push_back(A);
        SetIndices[SetIndex].push_back(B);
        SetRestValues[SetIndex].push_back(Distance(Particles[A], Particles[B]));
    };

    // Horizontal structural constraints.
    for (int32 Y = 0; Y < GridDesc.Height; ++Y)
    {
        for (int32 X = 0; X + 1 < GridDesc.Width; ++X)
        {
            const uint32 A = static_cast<uint32>(Y * GridDesc.Width + X);
            AddConstraintToSet((X % 2 == 0) ? 0 : 1, A, A + 1);
        }
    }

    // Vertical structural constraints.
    for (int32 Y = 0; Y + 1 < GridDesc.Height; ++Y)
    {
        for (int32 X = 0; X < GridDesc.Width; ++X)
        {
            const uint32 A = static_cast<uint32>(Y * GridDesc.Width + X);
            AddConstraintToSet((Y % 2 == 0) ? 2 : 3, A, A + GridDesc.Width);
        }
    }

    if (ConstraintSettings.bUseShear)
    {
        for (int32 Y = 0; Y + 1 < GridDesc.Height; ++Y)
        {
            for (int32 X = 0; X + 1 < GridDesc.Width; ++X)
            {
                const uint32 I0 = static_cast<uint32>(Y * GridDesc.Width + X);
                const uint32 I1 = I0 + 1;
                const uint32 I2 = I0 + GridDesc.Width;
                const uint32 I3 = I2 + 1;
                AddConstraintToSet(4 + ((X + Y) % 2), I0, I3);
                AddConstraintToSet(6 + ((X + Y) % 2), I1, I2);
            }
        }
    }

    if (ConstraintSettings.bUseBending)
    {
        for (int32 Y = 0; Y < GridDesc.Height; ++Y)
        {
            for (int32 X = 0; X + 2 < GridDesc.Width; ++X)
            {
                const uint32 A = static_cast<uint32>(Y * GridDesc.Width + X);
                AddConstraintToSet(8 + (X % 2), A, A + 2);
            }
        }

        for (int32 Y = 0; Y + 2 < GridDesc.Height; ++Y)
        {
            for (int32 X = 0; X < GridDesc.Width; ++X)
            {
                const uint32 A = static_cast<uint32>(Y * GridDesc.Width + X);
                AddConstraintToSet(10 + (Y % 2), A, A + 2 * GridDesc.Width);
            }
        }
    }

    for (int32 SetIndex = 0; SetIndex < ConstraintSetCount; ++SetIndex)
    {
        Indices.insert(Indices.end(), SetIndices[SetIndex].begin(), SetIndices[SetIndex].end());
        RestValues.insert(RestValues.end(), SetRestValues[SetIndex].begin(), SetRestValues[SetIndex].end());
        Sets.push_back(static_cast<uint32>(RestValues.size()));
        PhaseIndices.push_back(static_cast<uint32>(SetIndex));
    }
}

void FClothInstance::BuildGridTriangles(const FClothGridDesc& GridDesc)
{
    for (int32 Y = 0; Y + 1 < GridDesc.Height; ++Y)
    {
        for (int32 X = 0; X + 1 < GridDesc.Width; ++X)
        {
            const uint32 I0 = static_cast<uint32>(Y * GridDesc.Width + X);
            const uint32 I1 = I0 + 1;
            const uint32 I2 = I0 + GridDesc.Width;
            const uint32 I3 = I2 + 1;

            Triangles.push_back(I0);
            Triangles.push_back(I1);
            Triangles.push_back(I2);

            Triangles.push_back(I1);
            Triangles.push_back(I3);
            Triangles.push_back(I2);
        }
    }
}

bool FClothInstance::CreateNvClothObjects(const FClothBuildDesc& BuildDesc)
{
    nv::cloth::Factory* Factory = FNvClothSDK::Get().GetFactory();
    if (!Factory)
    {
        UE_LOG("[NvCloth] Cannot create cloth because Factory is null");
        return false;
    }

    Fabric = Factory->createFabric(
        static_cast<uint32>(Particles.size()),
        MakeConstRange(PhaseIndices),
        MakeConstRange(Sets),
        MakeConstRange(RestValues),
        MakeConstRange(StiffnessValues),
        MakeConstRange(Indices),
        MakeConstRange(Anchors),
        MakeConstRange(TetherLengths),
        MakeConstRange(Triangles));

    if (!Fabric)
    {
        UE_LOG("[NvCloth] Failed to create Fabric");
        return false;
    }

    Cloth = Factory->createCloth(MakeConstRange(Particles), *Fabric);
    if (!Cloth)
    {
        Fabric->decRefCount();
        Fabric = nullptr;
        UE_LOG("[NvCloth] Failed to create Cloth");
        return false;
    }

    ApplySimulationSettings(BuildDesc.SimulationSettings, BuildDesc.ConstraintSettings);
    UpdateCollision(FClothCollisionData{});
    UpdateRenderVerticesFromParticles();
    return true;
}

void FClothInstance::Release()
{
    if (Cloth)
    {
        delete Cloth;
        Cloth = nullptr;
    }

    if (Fabric)
    {
        Fabric->decRefCount();
        Fabric = nullptr;
    }

    ClearBuildData();
}

void FClothInstance::ApplySimulationSettings(const FClothSimulationSettings& Settings, const FClothConstraintSettings& InConstraintSettings)
{
    SimulationSettings = Settings;
    ConstraintSettings = InConstraintSettings;

    if (!Cloth)
    {
        return;
    }

    Cloth->setGravity(ToPxVec3(Settings.Gravity));
    Cloth->setSolverFrequency(Settings.SolverFrequency);
    Cloth->setStiffnessFrequency(Settings.StiffnessFrequency);
    Cloth->setCollisionMassScale(Settings.CollisionMassScale);
    Cloth->enableContinuousCollision(Settings.bEnableCCD);
    Cloth->setFriction(Settings.Friction);
    Cloth->setDamping(ToPxVec3(Settings.Damping));
    Cloth->setLinearDrag(ToPxVec3(Settings.LinearDrag));
    Cloth->setAngularDrag(ToPxVec3(Settings.AngularDrag));

    TArray<nv::cloth::PhaseConfig> PhaseConfigs;
    PhaseConfigs.reserve(PhaseIndices.size());

    for (uint32 PhaseIndex : PhaseIndices)
    {
        nv::cloth::PhaseConfig Config(static_cast<uint16>(PhaseIndex));
        if (PhaseIndex < 4)
        {
            Config.mStiffness = ConstraintSettings.StructuralStiffness;
        }
        else if (PhaseIndex < 8)
        {
            Config.mStiffness = ConstraintSettings.ShearStiffness;
        }
        else
        {
            Config.mStiffness = ConstraintSettings.BendingStiffness;
        }

        Config.mStiffnessMultiplier = 1.0f;
        Config.mCompressionLimit = 1.0f;
        Config.mStretchLimit = 1.0f;
        PhaseConfigs.push_back(Config);
    }

    if (!PhaseConfigs.empty())
    {
        Cloth->setPhaseConfig(MakeConstRange(PhaseConfigs));
    }
}

void FClothInstance::UpdateCollision(const FClothCollisionData& CollisionData)
{
    if (!Cloth)
    {
        return;
    }

    // 캡슐은 구를 참조하고, 볼록 다면체(convex)는 평면을 참조하므로 의존하는 데이터를 먼저 소거한다.
    Cloth->setCapsules(nv::cloth::Range<const uint32>(), 0, Cloth->getNumCapsules());
    Cloth->setConvexes(nv::cloth::Range<const uint32>(), 0, Cloth->getNumConvexes());

    Cloth->setSpheres(MakeConstRange(CollisionData.Spheres), 0, Cloth->getNumSpheres());
    Cloth->setPlanes(MakeConstRange(CollisionData.Planes), 0, Cloth->getNumPlanes());
    Cloth->setCapsules(MakeConstRange(CollisionData.Capsules), 0, 0);
    Cloth->setConvexes(MakeConstRange(CollisionData.ConvexMasks), 0, 0);
}

void FClothInstance::SetSimulationSpaceTransform(const FVector& WorldLocation, const FQuat& WorldRotation, bool bTeleport)
{
    if (!Cloth)
    {
        return;
    }

    const physx::PxVec3 PxLocation = ToPxVec3(WorldLocation);
    const physx::PxQuat PxRotation = ToPxQuat(WorldRotation);

	// 천이 순간이동했을 시, 누적된 가속도 및 속도로 인해 천이 비정상적으로 튀지 않도록 관성을 제거한다.
    if (bTeleport)
    {
        Cloth->teleportToLocation(PxLocation, PxRotation);
        Cloth->clearInertia();
        return;
    }

    Cloth->setTranslation(PxLocation);
    Cloth->setRotation(PxRotation);
}

void FClothInstance::PostSceneSimulate()
{
    UpdateRenderVerticesFromParticles();
}

// 렌더링 정점 데이터(FVertexPNCTT)를 입자 위치로부터 업데이트하는 함수. 입자 시뮬레이션 결과를 렌더링에 반영하기 위해 매 프레임 호출.
//   - Normal: 입자들이 구성하는 삼각형의 두 엣지를 외적하여 면 법선을 구하고, 구성 정점에 누적한 뒤 정규화한다.
//   - UV: 소스 메시의 속성을 사용하거나 그리드 위치에 따라 계산한다. 
//   - Offset: 시뮬레이션 설정에 따라 법선 방향으로 정점 위치를 이동한다.
//   - Tangent: 소스 메시의 탄젠트가 있으면 사용하고, 없으면 그리드의 수평 엣지를 따라 계산한다.
void FClothInstance::UpdateRenderVerticesFromParticles()
{
    if (!Cloth)
    {
        return;
    }

    auto CurrentParticles = Cloth->getCurrentParticles();
    RenderVertices.resize(CurrentParticles.size());

    for (uint32 i = 0; i < CurrentParticles.size(); ++i)
    {
        const physx::PxVec4& P = CurrentParticles[i];
        FVertexPNCTT& V = RenderVertices[i];
        V.Position = FVector(P.x, P.y, P.z);
        V.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

        if (bUseSourceMeshAttributes && i < SourceUVs.size())
        {
            V.UV = SourceUVs[i];
        }
        else
        {
            V.UV = FVector2(
                GridWidth > 1 ? static_cast<float>(i % GridWidth) / static_cast<float>(GridWidth - 1) : 0.0f,
                GridWidth > 0 && GridHeight > 1 ? static_cast<float>(i / GridWidth) / static_cast<float>(GridHeight - 1) : 0.0f);
        }
    }

    TArray<FVector> NormalSums;
    NormalSums.resize(RenderVertices.size(), FVector(0.0f, 0.0f, 0.0f));

    for (uint32 i = 0; i + 2 < Triangles.size(); i += 3)
    {
        const uint32 I0 = Triangles[i + 0];
        const uint32 I1 = Triangles[i + 1];
        const uint32 I2 = Triangles[i + 2];
        if (I0 >= RenderVertices.size() || I1 >= RenderVertices.size() || I2 >= RenderVertices.size())
        {
            continue;
        }

        const FVector& P0 = RenderVertices[I0].Position;
        const FVector& P1 = RenderVertices[I1].Position;
        const FVector& P2 = RenderVertices[I2].Position;
        const FVector FaceNormal = (P1 - P0).Cross(P2 - P0);

        if (LengthSq(FaceNormal) > 1.0e-8f)
        {
            NormalSums[I0] += FaceNormal;
            NormalSums[I1] += FaceNormal;
            NormalSums[I2] += FaceNormal;
        }
    }

    for (uint32 i = 0; i < RenderVertices.size(); ++i)
    {
        FVector N = NormalSums[i];
        if (LengthSq(N) > 1.0e-8f)
        {
            N.Normalize();
        }
        else
        {
            N = FVector(0.0f, 0.0f, 1.0f);
        }
        RenderVertices[i].Normal = N;
    }

    if (bUseSourceMeshAttributes)
    {
        for (uint32 i = 0; i < RenderVertices.size(); ++i)
        {
            RenderVertices[i].Tangent = i < SourceTangents.size() ? SourceTangents[i] : FVector4(1.0f, 0.0f, 0.0f, 1.0f);
        }
    }
    else
    {
        auto GetIndex = [this](int32 X, int32 Y)
        {
            return static_cast<uint32>(Y * GridWidth + X);
        };

        for (int32 Y = 0; Y < GridHeight; ++Y)
        {
            for (int32 X = 0; X < GridWidth; ++X)
            {
                const int32 X0 = (std::max)(0, X - 1);
                const int32 X1 = (std::min)(GridWidth - 1, X + 1);
                FVector Tangent = RenderVertices[GetIndex(X1, Y)].Position - RenderVertices[GetIndex(X0, Y)].Position;
                if (LengthSq(Tangent) > 1.0e-8f)
                {
                    Tangent.Normalize();
                }
                else
                {
                    Tangent = FVector(1.0f, 0.0f, 0.0f);
                }
                RenderVertices[GetIndex(X, Y)].Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, 1.0f);
            }
        }
    }

    if (SimulationSettings.RenderNormalOffset > 0.0f)
    {
        for (FVertexPNCTT& Vertex : RenderVertices)
        {
            Vertex.Position += Vertex.Normal * SimulationSettings.RenderNormalOffset;
        }
    }

    ++RenderRevision;
}

bool FClothInstance::GetLocalBounds(FVector& OutCenter, FVector& OutExtent) const
{
    if (RenderVertices.empty())
    {
        return false;
    }

    FVector Min = RenderVertices[0].Position;
    FVector Max = RenderVertices[0].Position;
    for (const FVertexPNCTT& V : RenderVertices)
    {
        Min.X = (std::min)(Min.X, V.Position.X);
        Min.Y = (std::min)(Min.Y, V.Position.Y);
        Min.Z = (std::min)(Min.Z, V.Position.Z);
        Max.X = (std::max)(Max.X, V.Position.X);
        Max.Y = (std::max)(Max.Y, V.Position.Y);
        Max.Z = (std::max)(Max.Z, V.Position.Z);
    }

    OutCenter = (Min + Max) * 0.5f;
    OutExtent = (Max - Min) * 0.5f;
    return true;
}
