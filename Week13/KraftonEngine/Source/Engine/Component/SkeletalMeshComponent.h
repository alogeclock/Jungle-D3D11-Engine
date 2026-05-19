#pragma once

#include "SkinnedMeshComponent.h"
#include "Animation/AnimInstance.h"

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
	void Serialize(FArchive& Ar) override;

	// AnimationInstance 관리
	void SetAnimInstance(UAnimInstance* InInstance);
	UAnimInstance* GetAnimInstance() const { return AnimInstance; }

	void SetTwoBoneIKEnabled(bool bEnabled) { bEnableTwoBoneIK = bEnabled; }
	bool IsTwoBoneIKEnabled() const { return bEnableTwoBoneIK; }
	void SetTwoBoneIKChains(const TArray<FTwoBoneIKChain>& InChains);
	void AddTwoBoneIKChain(const FTwoBoneIKChain& Chain);
	void ClearTwoBoneIKChains();
	const TArray<FTwoBoneIKChain>& GetTwoBoneIKChains() const { return TwoBoneIKChains; }
	bool SetIKTargetPosition(int32 ChainIndex, const FVector& WorldPosition);
	int32 FindBoneIndexByName(const FString& BoneName) const;

	// Notify 실행을 위해서 AnimInstance에서 여기로 전달하고 GetOwner로 전달
	void HandleAnimNotify(const FAnimNotifyEvent& Notify);

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction& ThisTickFunction) override;

private:
	// AnimationInstance의 포즈 BoneEditLocalMatrices에 복사
	void ApplyPoseToComponent(const FPoseContext& Pose);
	void ApplyTwoBoneIKChains(FPoseContext& Pose);

	void SolveTwoBoneIK(FPoseContext& Pose, int RootBoneIndex, int MidBoneIndex, int EndBoneIndex, const FVector& TargetPosition, const FVector& PolePosition);

	bool bEnableTwoBoneIK = true;
	TArray<FTwoBoneIKChain> TwoBoneIKChains;
	UAnimInstance* AnimInstance = nullptr;
};
