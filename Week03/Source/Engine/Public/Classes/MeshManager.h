#pragma once

#include "Source/Core/Public/Math/Box.h"
#include "Source/Engine/Object/Public/Object.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include "Source/Engine/Public/VertexData.h"

class URenderer;

struct FMeshData
{
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 NumVertices = 0;
    uint32 NumIndices = 0;
    uint32 Stride = 0;
};

class UMeshManager : public UObject
{
public:
    static UMeshManager& Get()
    {
        static UMeshManager instance("MeshManagerInstance");
        return instance;
    }

    UMeshManager(const FString& InString);
    ~UMeshManager() {};

    void Initialize(URenderer& Renderer);
    void Release(URenderer& Renderer);

    template <typename TVertex>
    void RegisterPrimitive(URenderer& Renderer, EPrimitiveType Type, TArray<TVertex>* VerticesPtr)
    {
        if (!VerticesPtr || VerticesPtr->empty())
            return;

        // 1. 원본 데이터 포인터를 void*로 업캐스팅하여 저장
        VertexData.emplace(Type, static_cast<void*>(VerticesPtr));
        uint32 ByteWidth = static_cast<uint32>(VerticesPtr->size()) * sizeof(TVertex);
        VertexBuffers.emplace(Type, Renderer.CreateVertexBuffer(VerticesPtr->data(), ByteWidth));
        NumVertices.emplace(Type, static_cast<uint32>(VerticesPtr->size()));
        MeshAABB.emplace(Type, ComputeAABB(*VerticesPtr));
        Strides.emplace(Type, sizeof(TVertex));
    }

    TArray<FTextureVertex> RebuildMesh();

    template <class TVertex> FBox ComputeAABB(const TArray<TVertex>& Vertices)
    {
        FBox Box;

        for (const auto& V : Vertices)
            Box.Encapsulate(V.Position);

        return Box;
    }
    FBox GetMeshAABB(EPrimitiveType Type) const;

    ID3D11Buffer* GetVertexBuffer(EPrimitiveType Type) const;
    uint32 GetNumVertices(EPrimitiveType Type) const;

    template <class TVertex> TArray<TVertex>* GetVertexData(EPrimitiveType Type) const
    {
        auto it = VertexData.find(Type);
        if (it != VertexData.end())
            return static_cast<TArray<TVertex>*>(it->second);

        return nullptr;
    }

    ID3D11Buffer* GetIndexBuffer(EPrimitiveType Type) const;
    uint32 GetNumIndices(EPrimitiveType Type) const;
    TArray<uint16>* GetIndexData(EPrimitiveType Type) const;

    uint32 GetStride(EPrimitiveType Type) const
    {
        auto it = Strides.find(Type);
        if (it != Strides.end())
        {
            return it->second;
        }
        
        // 등록되지 않은 타입이거나 오류가 발생한 경우 기본값 0 반환
        return 0;
    }

private:
    TMap<EPrimitiveType, ID3D11Buffer*> VertexBuffers;
    TMap<EPrimitiveType, uint32> NumVertices;
    TMap<EPrimitiveType, void*> VertexData;
    TMap<EPrimitiveType, uint32> Strides;

        TMap<EPrimitiveType, ID3D11Buffer*> IndexBuffers;
    TMap<EPrimitiveType, uint32> NumIndices;
    TMap<EPrimitiveType, TArray<uint16>*> IndexData;
    TMap<EPrimitiveType, FBox> MeshAABB;
};