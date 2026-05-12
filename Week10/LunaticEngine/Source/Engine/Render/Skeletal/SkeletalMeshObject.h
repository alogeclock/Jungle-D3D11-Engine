#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Render/Types/VertexTypes.h"

class FMeshBuffer;
struct FSkeletalMesh;

// FSkeletalMeshObject 
// GT(Component)가 본 행렬을 평가한 뒤, 결과를 RT 측에 넘겨주는 핸드오프 객체.
//
//   FSkeletalMeshObjectCPU — CPU 스키닝 (현 단계)
//   FSkeletalMeshObjectGPU — Compute shader 기반 
class FSkeletalMeshObject
{
public:
	virtual ~FSkeletalMeshObject() = default;

	// Skinning matrix 팔레트를 받아 정점 변형 + GPU 업로드
	virtual void Update(const TArray<FMatrix>& InSkinningMatrices) = 0;

	// 프록시가 매 프레임 가져갈 결과
	virtual FMeshBuffer* GetMeshBuffer() const = 0;

	// Component hit-test/editing paths need the current deformed geometry, not bind-pose vertices.
	virtual FMeshDataView GetMeshDataView() const
	{
		return {}; 
	}
};
