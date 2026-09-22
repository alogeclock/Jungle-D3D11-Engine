#include "Engine/Source/Runtime/Engine/Public/Classes/MeshManager.h"

UMeshManager::UMeshManager(const FString &InString) : UObject(InString) {}

void UMeshManager::Initialize(URenderer &Renderer)
{
    int   GridSize = 1000;  // 100x100 칸
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

    VertexData.emplace(EPrimitiveType::Cube, &cube_vertices);
    VertexData.emplace(EPrimitiveType::Sphere, &sphere_vertices);
    VertexData.emplace(EPrimitiveType::Triangle, &triangle_vertices);
    VertexData.emplace(EPrimitiveType::Plane, &plane_vertices);
    VertexData.emplace(EPrimitiveType::Arrow, &arrow_vertices);
    VertexData.emplace(EPrimitiveType::CubeArrow, &cube_arrow_vertices);
    VertexData.emplace(EPrimitiveType::Ring, &ring_vertices);
    VertexData.emplace(EPrimitiveType::Axis, &axis_vertices);
    VertexData.emplace(EPrimitiveType::Grid, &grid_vertices);

    VertexBuffers.emplace(EPrimitiveType::Cube, Renderer.CreateVertexBuffer(cube_vertices.data(), static_cast<int>(cube_vertices.size()) * sizeof(FVertex)));
    VertexBuffers.emplace(EPrimitiveType::Sphere,
                          Renderer.CreateVertexBuffer(sphere_vertices.data(), static_cast<int>(sphere_vertices.size() * sizeof(FVertex))));
    VertexBuffers.emplace(EPrimitiveType::Triangle,
                          Renderer.CreateVertexBuffer(triangle_vertices.data(), static_cast<int>(triangle_vertices.size() * sizeof(FVertex))));
    VertexBuffers.emplace(EPrimitiveType::Plane, Renderer.CreateVertexBuffer(plane_vertices.data(), static_cast<int>(plane_vertices.size() * sizeof(FVertex))));
    VertexBuffers.emplace(EPrimitiveType::Arrow, Renderer.CreateVertexBuffer(arrow_vertices.data(), static_cast<int>(arrow_vertices.size() * sizeof(FVertex))));
    VertexBuffers.emplace(EPrimitiveType::CubeArrow,
                          Renderer.CreateVertexBuffer(cube_arrow_vertices.data(), static_cast<int>(cube_arrow_vertices.size() * sizeof(FVertex))));
    VertexBuffers.emplace(EPrimitiveType::Ring, Renderer.CreateVertexBuffer(ring_vertices.data(), static_cast<int>(ring_vertices.size() * sizeof(FVertex))));
    VertexBuffers.emplace(EPrimitiveType::Axis, Renderer.CreateVertexBuffer(axis_vertices.data(), static_cast<int>(axis_vertices.size() * sizeof(FVertex))));
    VertexBuffers.emplace(EPrimitiveType::Grid, Renderer.CreateVertexBuffer(grid_vertices.data(), static_cast<int>(grid_vertices.size() * sizeof(FVertex))));

    NumVertices.emplace(EPrimitiveType::Cube, static_cast<uint32>(cube_vertices.size()));
    NumVertices.emplace(EPrimitiveType::Sphere, static_cast<uint32>(sphere_vertices.size()));
    NumVertices.emplace(EPrimitiveType::Triangle, static_cast<uint32>(triangle_vertices.size()));
    NumVertices.emplace(EPrimitiveType::Plane, static_cast<uint32>(plane_vertices.size()));
    NumVertices.emplace(EPrimitiveType::Arrow, static_cast<uint32>(arrow_vertices.size()));
    NumVertices.emplace(EPrimitiveType::CubeArrow, static_cast<uint32>(cube_arrow_vertices.size()));
    NumVertices.emplace(EPrimitiveType::Ring, static_cast<uint32>(ring_vertices.size()));
    NumVertices.emplace(EPrimitiveType::Axis, static_cast<uint32>(axis_vertices.size()));
    NumVertices.emplace(EPrimitiveType::Grid, static_cast<uint32>(grid_vertices.size()));


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
