#pragma once

#include "Source/Engine/Public/Rendering/Renderer.h"
#include "Source/Engine/Public/VertexData.h"
#include "Source/Engine/Object/Public/Object.h"
#include "Source/Core/Public/Math/Box.h"

class URenderer;

class UMeshManager : public UObject
{
  public:
    static UMeshManager &Get()
    {
        static UMeshManager instance("MeshManagerInstance");
        return instance;
    }

    UMeshManager(const FString &InString);
    ~UMeshManager() {};

    void Initialize(URenderer &Renderer);
    void Release(URenderer &Renderer);

    FBox ComputeAABB(const TArray<FVertex> &Vertices);
    FBox GetMeshAABB(EPrimitiveType Type) const;

    ID3D11Buffer    *GetVertexBuffer(EPrimitiveType Type) const;
    uint32           GetNumVertices(EPrimitiveType Type) const;
    TArray<FVertex> *GetVertexData(EPrimitiveType Type) const;

    ID3D11Buffer *GetIndexBuffer(EPrimitiveType Type) const;
    uint32        GetNumIndices(EPrimitiveType Type) const;
    TArray<uint16> *GetIndexData(EPrimitiveType Type) const;

  private:
    TMap<EPrimitiveType, ID3D11Buffer *>    VertexBuffers;
    TMap<EPrimitiveType, uint32>            NumVertices;
    TMap<EPrimitiveType, TArray<FVertex> *> VertexData;

    TMap<EPrimitiveType, ID3D11Buffer *> IndexBuffers;
    TMap<EPrimitiveType, uint32>         NumIndices;
    TMap<EPrimitiveType, TArray<uint16> *> IndexData;
    TMap<EPrimitiveType, FBox>            MeshAABB;
};