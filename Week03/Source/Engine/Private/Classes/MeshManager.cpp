#include "Source/Engine/Public/Classes/MeshManager.h"


UMeshManager::UMeshManager(const FString& InString) : UObject(InString)
{
}

void UMeshManager::Initialize(URenderer& Renderer)
{
    TArray<EPrimitiveType> PrimitiveTypes = {EPrimitiveType::Cube,  EPrimitiveType::Sphere, EPrimitiveType::Triangle,
                                             EPrimitiveType::Plane, EPrimitiveType::Arrow,  EPrimitiveType::CubeArrow,
                                             EPrimitiveType::Ring,  EPrimitiveType::TextureQuad};

    TArray<TArray<FVertex>*> VerticesPtr = {&cube_vertices,  &sphere_vertices, &triangle_vertices,
                                            &plane_vertices, &arrow_vertices,  &cube_arrow_vertices,
                                            &ring_vertices,  &texture_quad_vertices};

    for (int i = 0; i < PrimitiveTypes.size(); i++)
    {
        EPrimitiveType t = PrimitiveTypes[i];
        TArray<FVertex>* vptr = VerticesPtr[i];

        VertexData.emplace(t, vptr);
        VertexBuffers.emplace(
            t, Renderer.CreateVertexBuffer(vptr->data(), static_cast<int>(vptr->size()) * sizeof(FVertex)));
        NumVertices.emplace(t, static_cast<uint32>(vptr->size()));
        MeshAABB.emplace(t, ComputeAABB(*vptr));
    }

    IndexData.emplace(EPrimitiveType::Sphere, &sphere_indices);
    IndexBuffers.emplace(
        EPrimitiveType::Sphere,
        Renderer.CreateIndexBuffer(sphere_indices.data(), static_cast<int>(sphere_indices.size() * sizeof(uint16))));
    NumIndices.emplace(EPrimitiveType::Sphere, static_cast<uint32>(sphere_indices.size()));

    TArray<TArray<FTextureVertex>*> TextureVerticesPtr = {&billboard_vertices};
    TArray<EPrimitiveType> TexturePrimitiveTypes = {EPrimitiveType::Billboard};
    for (int i = 0; i < TexturePrimitiveTypes.size(); i++)
    {
        EPrimitiveType t = TexturePrimitiveTypes[i];
        TArray<FTextureVertex>* vptr = TextureVerticesPtr[i];

        TextureVertexData.emplace(t, vptr);
        TextureVertexBuffers.emplace(
            t, Renderer.CreateTextureVertexBuffer(vptr->data(), static_cast<int>(vptr->size()) * sizeof(FTextureVertex)));
        NumTextureVertices.emplace(t, static_cast<uint32>(vptr->size()));
    }

    TArray<FTextureVertex> VerticesSubUV = RebuildMesh();

    TextureVertexData.emplace(EPrimitiveType::SubUV, &VerticesSubUV);

    VertexBuffers.emplace(EPrimitiveType::SubUV,
        Renderer.CreateVertexBuffer(
            VerticesSubUV.data(),
            static_cast<int>(VerticesSubUV.size()) * sizeof(FTextureVertex)
        )
    );

    NumVertices.emplace(EPrimitiveType::SubUV,
        static_cast<uint32>(VerticesSubUV.size())
    );
}

TArray<FTextureVertex> UMeshManager::RebuildMesh()
{
    TArray<FTextureVertex> VerticesSubUV;
    VerticesSubUV.clear();

    uint32 SpriteSize = 150;
    uint32 Height = 900;
    uint32  Width = 900;

    uint32 Row = Height / SpriteSize;
    uint32 Collum = Width / SpriteSize;

    uint32 Size = Row * Collum;
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

        VerticesSubUV.push_back({FVector<float>(x0, y0, 0.f), fu0, fv0});
        VerticesSubUV.push_back({FVector<float>(x1, y0, 0.f), fu1, fv0});
        VerticesSubUV.push_back({FVector<float>(x0, y1, 0.f), fu0, fv1});

        VerticesSubUV.push_back({FVector<float>(x1, y0, 0.f), fu1, fv0});
        VerticesSubUV.push_back({FVector<float>(x1, y1, 0.f), fu1, fv1});
        VerticesSubUV.push_back({FVector<float>(x0, y1, 0.f), fu0, fv1});
    }
    return VerticesSubUV;
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

    for (auto& Pair : TextureVertexBuffers)
    {
        renderer.ReleaseTextureVertexBuffer(Pair.second);
    }
}

FBox UMeshManager::ComputeAABB(const TArray<FVertex>& Vertices)
{
    FBox Box;

    for (const auto& V : Vertices)
        Box.Encapsulate(V.Position);

    return Box;
}

FBox UMeshManager::ComputeAABB(const TArray<FTextureVertex>& Vertices)
{
    FBox Box;

    for (const auto& V : Vertices)
        Box.Encapsulate(V.Position);

    return Box;
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

TArray<FVertex>* UMeshManager::GetVertexData(EPrimitiveType Type) const
{
    return VertexData.at(Type);
}

TArray<FTextureVertex>* UMeshManager::GetTextureVertexData(EPrimitiveType Type) const
{
    return TextureVertexData.at(Type);
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

uint32 UMeshManager::GetNumTextureVertices(EPrimitiveType Type) const
{
    auto it = NumTextureVertices.find(Type);
    if (it == NumTextureVertices.end())
        return 0;
    return it->second;
}

ID3D11Buffer* UMeshManager::GetTextureVertexBuffer(EPrimitiveType Type) const
{
    auto it = TextureVertexBuffers.find(Type);
    if (it == TextureVertexBuffers.end())
        return nullptr;
    return it->second;
}