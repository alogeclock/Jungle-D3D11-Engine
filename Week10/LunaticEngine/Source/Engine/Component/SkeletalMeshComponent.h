#pragma once

#include "Component/SkinnedMeshComponent.h"
#include "Core/PropertyTypes.h"
#include "Mesh/SkeletalMesh.h"

class UMaterial;
class FPrimitiveSceneProxy;

namespace json { class JSON; }

// USkinnedMeshComponent에 Animation으로 BoneSpaceTransforms를 채우는 책임을 추가
// 현재 단계에서는 AnimInstance가 없어 base의 RefPose fallback을 그대로 씀
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	void RefreshBoneTransforms() override;

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:

};