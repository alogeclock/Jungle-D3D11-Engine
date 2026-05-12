#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class USkinnedMeshComponent;

// ============================================================
// FSkeletalMeshSceneProxy — USkinnedMeshComponent 계열 전용 프록시
// ============================================================
// FSkeletalMesh::LODModels[lod].Sections를 기반으로 SectionDraws 구축.
// 현재 CPU 스키닝 결과 버퍼와 LOD0 섹션 정보를 묶어 draw command용 데이터를 만든다.
// GPU 스키닝/LOD별 버퍼는 후속 단계에서 확장한다.
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
