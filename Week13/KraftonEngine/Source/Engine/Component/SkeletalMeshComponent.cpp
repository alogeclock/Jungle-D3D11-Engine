#include "SkeletalMeshComponent.h"
#include "Core/Log.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Mesh/SkeletalMesh.h"
#include "GameFramework/AActor.h"
#include <cctype>

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
	AnimInstance->TriggerAnimNotifies();

	if (!Pose.BoneLocalTransforms.empty())
		ApplyPoseToComponent(Pose);
}

void USkeletalMeshComponent::ApplyPoseToComponent(const FPoseContext& Pose)
{
	if (!SkeletalMesh)
		return;

	EnsureBoneEditPose();

	const FSkeletonAsset* SkeletonAsset = SkeletalMesh->GetSkeletonAsset();

	for (uint32 BoneIdx = 0; BoneIdx < static_cast<uint32>(Pose.BoneLocalTransforms.size()); ++BoneIdx)
	{
		if (BoneIdx >= static_cast<uint32>(BoneEditLocalMatrices.size()))
			continue;

		FTransform BoneTM = Pose.BoneLocalTransforms[BoneIdx];

		BoneEditLocalMatrices[BoneIdx] = BoneTM.ToMatrix();
	}

	bUseBoneEditPose = true;
	UpdateCPUSkinning();
	++SkinnedRevision;
}

void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
	if (!Notify.NotifyTrigger)
		return;

	AActor* Owner = GetOwner();
	Notify.NotifyTrigger->OnNotify(Owner, this);
}
