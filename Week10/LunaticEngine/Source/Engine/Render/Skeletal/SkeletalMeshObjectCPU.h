#pragma once

#include "Render/Skeletal/SkeletalMeshObject.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/VertexTypes.h"

#include <memory>

struct ID3D11Device;
struct FSkeletalMesh;

// FSkeletalMeshObjectCPU — CPU에서 정점을 스키닝하고 FMeshBuffer를 매 프레임 재구축.
// 비용: 매 프레임 정점 순회 + DX11 버퍼 Create. 프로토타입용. 추후 동적 VB로 최적화.
class FSkeletalMeshObjectCPU : public FSkeletalMeshObject
{
public:
    // Source: USkeletalMesh가 들고 있는 FSkeletalMesh*. 외부 소유, 약한 참조.
    // Device: GPU 버퍼 생성용. 외부 소유.
    FSkeletalMeshObjectCPU(const FSkeletalMesh* InSource, ID3D11Device* InDevice);
    ~FSkeletalMeshObjectCPU() override = default;

    void Update(const TArray<FMatrix>& InSkinningMatrices) override;
    FMeshBuffer* GetMeshBuffer() const override { return MeshBuffer.get(); }

private:
    const FSkeletalMesh* Source = nullptr;
    ID3D11Device* Device = nullptr;

    // 매 프레임 채워지는 CPU 캐시 FVertexPNCTT 포맷 (StaticMesh와 동일 InputLayout 재활용)
    TMeshData<FVertexPNCTT> SkinnedMeshData;

    // 매 프레임 재생성되는 GPU 버퍼
    std::unique_ptr<FMeshBuffer> MeshBuffer;
};