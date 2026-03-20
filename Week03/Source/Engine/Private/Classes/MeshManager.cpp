#include "Source/Engine/Public/Classes/MeshManager.h"

UMeshManager::UMeshManager(const FString &InString) : UObject(InString) {}

void UMeshManager::Initialize(URenderer &Renderer)
{
    int   GridSize = 1000; // 100x100 칸
    float GridStep = 1.0f; // 1칸의 크기

    FVector4<float> Color = {0.3f, 0.3f, 0.3f, 0.2f};

    for (int i = -GridSize; i <= GridSize; ++i)
    {
        // 1. 가로선 (X축과 평행한 선, Z값을 변화시키며 배치)
        grid_vertices.push_back(FVertex{FVector<float>(-GridSize * GridStep, i * GridStep, -0.005f), Color});
        grid_vertices.push_back(FVertex{FVector<float>(GridSize * GridStep, i * GridStep, -0.005f), Color});

        // 2. 세로선 (Z축과 평행한 선, X값을 변화시키며 배치)
        grid_vertices.push_back(FVertex{FVector<float>(i * GridStep, -GridSize * GridStep, -0.005f), Color});
        grid_vertices.push_back(FVertex{FVector<float>(i * GridStep, GridSize * GridStep, -0.005f), Color});
    }

    TArray<EPrimitiveType> PrimitiveTypes = {EPrimitiveType::Cube,  EPrimitiveType::Sphere, EPrimitiveType::Triangle,
                                             EPrimitiveType::Plane, EPrimitiveType::Arrow,  EPrimitiveType::CubeArrow,
                                             EPrimitiveType::Ring,  EPrimitiveType::Axis,   EPrimitiveType::Grid};

    TArray<TArray<FVertex> *> VerticesPtr = {&cube_vertices,       &sphere_vertices, &triangle_vertices, &plane_vertices, &arrow_vertices,
                                             &cube_arrow_vertices, &ring_vertices,   &axis_vertices,     &grid_vertices};

    for (int i = 0; i < PrimitiveTypes.size(); i++)
    {
        EPrimitiveType   t = PrimitiveTypes[i];
        TArray<FVertex> *vptr = VerticesPtr[i];

        VertexData.emplace(t, vptr);
        VertexBuffers.emplace(t, Renderer.CreateVertexBuffer(vptr->data(), static_cast<int>(vptr->size()) * sizeof(FVertex)));
        NumVertices.emplace(t, static_cast<uint32>(vptr->size()));
    }

    IndexData.emplace(EPrimitiveType::Sphere, &sphere_indices);
    IndexBuffers.emplace(EPrimitiveType::Sphere, Renderer.CreateIndexBuffer(sphere_indices.data(), static_cast<int>(sphere_indices.size() * sizeof(uint16))));
    NumIndices.emplace(EPrimitiveType::Sphere, static_cast<uint32>(sphere_indices.size()));
}

void UMeshManager::Release(URenderer &renderer)
{
    for (auto &Pair : VertexBuffers)
    {
        renderer.ReleaseVertexBuffer(Pair.second);
    }
    // TMap.Empty()
    VertexBuffers.clear();

    for (auto &Pair : IndexBuffers)
    {
        renderer.ReleaseIndexBuffer(Pair.second);
    }
    // TMap.Empty()
    IndexBuffers.clear();
}

TArray<FVertex> *UMeshManager::GetVertexData(EPrimitiveType Type) const { return VertexData.at(Type); }

ID3D11Buffer *UMeshManager::GetVertexBuffer(EPrimitiveType Type) const { return VertexBuffers.at(Type); }

uint32 UMeshManager::GetNumVertices(EPrimitiveType Type) const { return NumVertices.at(Type); }

ID3D11Buffer *UMeshManager::GetIndexBuffer(EPrimitiveType Type) const
{
    auto it = IndexBuffers.find(Type);
    if (it == IndexBuffers.end())
        return nullptr; // 없으면 nullptr 반환
    return it->second;
}

uint32 UMeshManager::GetNumIndices(EPrimitiveType Type) const
{
    auto it = NumIndices.find(Type);
    if (it == NumIndices.end())
        return 0;
    return it->second;
}

TArray<uint16> *UMeshManager::GetIndexData(EPrimitiveType Type) const
{
    auto it = IndexData.find(Type);
    if (it == IndexData.end())
        return nullptr;
    return it->second;
}
