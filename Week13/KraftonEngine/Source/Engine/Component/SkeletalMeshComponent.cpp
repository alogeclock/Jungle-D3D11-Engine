#include "SkeletalMeshComponent.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Mesh/SkeletalMesh.h"
#include "GameFramework/AActor.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
	return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InInstance)
{
	AnimInstance = InInstance;
	if (AnimInstance)
		AnimInstance->Initialize(this);
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!AnimInstance)
		return;

	AnimInstance->Update(DeltaTime);

	FPoseContext Pose;
	AnimInstance->GetCurrentPose(Pose);
	if (!Pose.BoneLocalTransforms.empty())
		ApplyPoseToComponent(Pose);
}

void USkeletalMeshComponent::ApplyPoseToComponent(const FPoseContext& Pose)
{
	if (!SkeletalMesh)
		return;

	EnsureBoneEditPose();

	for (const auto& [BoneName, BoneIdx] : Pose.BoneNameToIndex)
	{
		if (BoneIdx < (uint32)BoneEditLocalMatrices.size())
			BoneEditLocalMatrices[BoneIdx] = Pose.BoneLocalTransforms[BoneIdx].ToMatrix();
	}

	bUseBoneEditPose = true;
	UpdateCPUSkinning();
	++SkinnedRevision;
}

void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
	AActor* Owner = GetOwner();
	if (Owner)
		Owner->OnAnimNotify(Notify.NotifyName);
}
