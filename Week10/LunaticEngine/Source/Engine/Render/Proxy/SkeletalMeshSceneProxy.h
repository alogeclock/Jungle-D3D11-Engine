#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class USkinnedMeshComponent;

// ============================================================
// FSkeletalMeshSceneProxy — USkeletalMeshComponent 전용 프록시
// ============================================================
// FSkeletalMesh::LODModels[lod].Sections를 기반으로 SectionDraws 구축.
// 현재는 RenderBuffer가 없어 실제 그려지지는 않지만, 머티리얼/섹션 매핑은 정상.
// Skinning / GPU 업로드는 후속 단계에서 추가.
class FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	static constexpr uint32 MAX_LOD = 4;

	FSkeletalMeshSceneProxy(USkinnedMeshComponent* InComponent);

	void UpdateMaterial() override;
	void UpdateMesh() override;
	void UpdateLOD(uint32 LODLevel) override;

private:
	USkinnedMeshComponent* GetSkinnedMeshComponent() const;

	void RebuildSectionDraws();

	struct FLODDrawData
	{
		FMeshBuffer* MeshBuffer = nullptr;
		TArray<FMeshSectionDraw> SectionDraws;
	};

	FLODDrawData LODData[MAX_LOD];
	uint32 LODCount = 1;
};
