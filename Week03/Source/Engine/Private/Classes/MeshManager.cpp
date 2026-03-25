#include "Source/Engine/Public/Classes/MeshManager.h"


UMeshManager::UMeshManager(const FString& InString) : UObject(InString)
{
}

void UMeshManager::Initialize(URenderer& Renderer)
{
    //TArray<EPrimitiveType> PrimitiveTypes = {EPrimitiveType::Cube,  EPrimitiveType::Sphere, EPrimitiveType::Triangle,
    //                                         EPrimitiveType::Plane, EPrimitiveType::Arrow,  EPrimitiveType::CubeArrow,
    //                                         EPrimitiveType::Ring,  EPrimitiveType::TextureQuad, EPrimitiveType::Billboard};

    RegisterPrimitive(Renderer, EPrimitiveType::Cube, &cube_vertices);
    RegisterPrimitive(Renderer, EPrimitiveType::Sphere, &sphere_vertices);
    RegisterPrimitive(Renderer, EPrimitiveType::Triangle, &triangle_vertices);
    RegisterPrimitive(Renderer, EPrimitiveType::Plane, &plane_vertices);
    RegisterPrimitive(Renderer, EPrimitiveType::Arrow, &arrow_vertices);
    RegisterPrimitive(Renderer, EPrimitiveType::CubeArrow, &cube_arrow_vertices);
    RegisterPrimitive(Renderer, EPrimitiveType::Ring, &ring_vertices);
    RegisterPrimitive(Renderer, EPrimitiveType::TextureQuad, &texture_quad_vertices);
    RegisterPrimitive(Renderer, EPrimitiveType::Billboard, &billboard_vertices);

    TArray<FTextureVertex> subuv_vertices = RebuildMesh();
    RegisterPrimitive(Renderer, EPrimitiveType::SubUV, &subuv_vertices);

    IndexData.emplace(EPrimitiveType::Sphere, &sphere_indices);
    IndexBuffers.emplace(
        EPrimitiveType::Sphere,
        Renderer.CreateIndexBuffer(sphere_indices.data(), static_cast<int>(sphere_indices.size() * sizeof(uint16))));
    NumIndices.emplace(EPrimitiveType::Sphere, static_cast<uint32>(sphere_indices.size()));
}

TArray<FTextureVertex> UMeshManager::RebuildMesh()
{
    Vertices.clear();

    uint32 SpriteSize = 150;
    uint32 Height = 900;
    uint32  Width = 900;

    uint32 Row = Height / SpriteSize;
    uint32 Collum = Width / SpriteSize;

    uint32 Size = Row * Collum;
    Vertices.reserve(Size * 6);
    //std::cout << Size << std::endl;

    for (int CurrentIndex = 0; CurrentIndex < Size; ++CurrentIndex)
    {
        uint32 u0, u1, v0, v1;
        u0 = CurrentIndex % Collum;
        v0 = CurrentIndex / Collum;

        u1 = u0 + 1;
        v1 = v0 + 1;

        u0 *= SpriteSize;
        u1 *= SpriteSize;
        v0 *= SpriteSize;
        v1 *= SpriteSize;

        float x0 = -0.5f;
        float x1 = 0.5f;
        float y0 = -0.5f;
        float y1 = 0.5f;

        float fu0 = (float)u0 / (float)Width;
        float fu1 = (float)u1 / (float)Width;
        float fv0 = (float)v0 / (float)Height;
        float fv1 = (float)v1 / (float)Height;

        Vertices.push_back({FVector<float>(x0, y0, 0.f), fu0, fv0});
        Vertices.push_back({FVector<float>(x1, y0, 0.f), fu1, fv0});
        Vertices.push_back({FVector<float>(x0, y1, 0.f), fu0, fv1});

        Vertices.push_back({FVector<float>(x1, y0, 0.f), fu1, fv0});
        Vertices.push_back({FVector<float>(x1, y1, 0.f), fu1, fv1});
        Vertices.push_back({FVector<float>(x0, y1, 0.f), fu0, fv1});
    }
}

void UMeshManager::Release(URenderer& renderer)
{
    for (auto& Pair : VertexBuffers)
    {
        renderer.ReleaseVertexBuffer(Pair.second);
    }
    // TMap.Empty()
    VertexBuffers.clear();

    for (auto& Pair : IndexBuffers)
    {
        renderer.ReleaseIndexBuffer(Pair.second);
    }
    // TMap.Empty()
    IndexBuffers.clear();
}

FBox UMeshManager::GetMeshAABB(EPrimitiveType Type) const
{
    auto it = MeshAABB.find(Type);

    if (it != MeshAABB.end())
    {
        return it->second;
    }

    return FBox(); // 찾지 못했을 경우 기본 생성된 박스 반환
}

ID3D11Buffer* UMeshManager::GetVertexBuffer(EPrimitiveType Type) const
{
    return VertexBuffers.at(Type);
}

uint32 UMeshManager::GetNumVertices(EPrimitiveType Type) const
{
    return NumVertices.at(Type);
}

ID3D11Buffer* UMeshManager::GetIndexBuffer(EPrimitiveType Type) const
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

TArray<uint16>* UMeshManager::GetIndexData(EPrimitiveType Type) const
{
    auto it = IndexData.find(Type);
    if (it == IndexData.end())
        return nullptr;
    return it->second;
}