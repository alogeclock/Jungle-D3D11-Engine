#pragma once

#include "SkinnedMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"

// SkeletalMesh 전용 render proxy만 제공하는 얇은 wrapper.
// Skinning/bone/material/bounds 상태는 모두 USkinnedMeshComponent가 소유한다.
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	// Render access 섹션: SceneProxy
	FPrimitiveSceneProxy* CreateSceneProxy() override;

private:
	void ApplyPoseToComponent(const FPoseContext& Pose);

	UAnimInstance* AnimInstance = nullptr;
	UAnimSingleNodeInstance* SingleNodeInstance = nullptr;
};
