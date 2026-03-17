#pragma once

#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Engine/Source/Runtime/Engine/Public/VertexData.h"
#include "Object/Object.h"

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
};