#include "Mesh/Skeleton.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(USkeleton, UObject)


int32 FReferenceSkeleton::FindBoneIndex(FName Name) const
{
	for (int32 i = 0; i < (int32)Bones.size(); ++i)
	{
		if (Bones[i].Name == Name)
		{
			return i;
		}
	}
	return -1;
}

void FReferenceSkeleton::Empty()
{
	Bones.clear();
	RefBonePose.clear();
	RefBasesInvMatrix.clear();
}

void FReferenceSkeleton::Allocate(int32 BoneCount)
{
	Bones.resize(BoneCount);
	RefBonePose.resize(BoneCount);
	RefBasesInvMatrix.assign(BoneCount, FMatrix::Identity);
}

void FReferenceSkeleton::RebuildRefBasesInvMatrix()
{
	const int32 Num = GetNum();
	if (Num == 0) { RefBasesInvMatrix.clear(); return; }
	TArray<FMatrix> ComponentSpace(Num, FMatrix::Identity);
	RefBasesInvMatrix.assign(Num, FMatrix::Identity);
	for (int32 i = 0; i < Num; ++i)
	{
		const FMatrix Local = RefBonePose[i].ToMatrix();
		const FMatrix Parent = (Bones[i].ParentIndex >= 0)
			? ComponentSpace[Bones[i].ParentIndex]
			: FMatrix::Identity;
		ComponentSpace[i] = Local * Parent;
		RefBasesInvMatrix[i] = ComponentSpace[i].GetInverse();
	}
}

void USkeleton::Serialize(FArchive& Ar)
{
}
