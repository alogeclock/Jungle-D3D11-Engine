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

	virtual void Update(const TArray<FMatrix>& InSkinningMatrices, const TArray<float>* MorphTargetWeights = nullptr) override;
    void SetLOD(uint32 LODIndex) override;
    uint32 GetLOD() const override { return CurrentLOD; }
    FMeshBuffer* GetMeshBuffer() const override { return MeshBuffer.get(); }
    FMeshDataView GetMeshDataView() const override { return FMeshDataView::FromMeshData(SkinnedMeshData); }
    bool GetSkinnedLocalBounds(FVector& OutCenter, FVector& OutExtent) const override;
	void ApplyMorphTargets(uint32 LODIndex,	const TArray<float>* Weights,TArray<FSkeletalVertex>& InOutVertices);
private:
    const FSkeletalMesh* Source = nullptr;
    ID3D11Device* Device = nullptr;
    uint32 CurrentLOD = 0;
	TArray<FSkeletalVertex> MorphedVertices;
    // 매 프레임 채워지는 CPU 캐시 FVertexPNCTT 포맷 (StaticMesh와 동일 InputLayout 재활용)
    TMeshData<FVertexPNCTT> SkinnedMeshData;

    // 매 프레임 재생성되는 GPU 버퍼
    std::unique_ptr<FMeshBuffer> MeshBuffer;

    FVector SkinnedLocalCenter = FVector(0.0f, 0.0f, 0.0f);
    FVector SkinnedLocalExtent = FVector(0.0f, 0.0f, 0.0f);
    bool bHasSkinnedLocalBounds = false;
};
