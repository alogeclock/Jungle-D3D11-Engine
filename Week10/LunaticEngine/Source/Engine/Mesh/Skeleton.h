#pragma once

#include "Object/Object.h"
#include "Object/FName.h"
#include "Math/Transform.h"
#include "Math/Matrix.h"
#include "Core/CoreTypes.h"

class FArchive;
struct FMeshBoneInfo
{
	FName Name;
	int32 ParentIndex = -1;  
};

// FReferenceSkeleton — 본 계층 + Bind Pose.
struct FReferenceSkeleton
{
	TArray<FMeshBoneInfo> Bones;
	TArray<FTransform>    RefBonePose;
	TArray<FMatrix>       RefBasesInvMatrix;

	int32 GetNum() const { return static_cast<int32>(Bones.size()); }
	int32 GetParentIndex(int32 BoneIndex) const { return Bones[BoneIndex].ParentIndex; }
	int32 FindBoneIndex(FName Name) const;
	bool  IsValidIndex(int32 i) const { return i >= 0 && i < GetNum(); }

	void Empty();
	void Allocate(int32 BoneCount);

	void RebuildRefBasesInvMatrix();
};


class USkeleton : public UObject
{
public:
	DECLARE_CLASS(USkeleton, UObject)

	USkeleton() = default;
	~USkeleton() override = default;

	void Serialize(FArchive& Ar);

	const FReferenceSkeleton& GetReferenceSkeleton() const { return RefSkeleton; }
	FReferenceSkeleton& GetReferenceSkeleton() { return RefSkeleton; }

	// 임포트 직후 임포터가 직접 호출하는 경로
	void SetReferenceSkeleton(FReferenceSkeleton&& In)
	{
		RefSkeleton = std::move(In);
		RefSkeleton.RebuildRefBasesInvMatrix();
	}

private:
	FReferenceSkeleton RefSkeleton;
};