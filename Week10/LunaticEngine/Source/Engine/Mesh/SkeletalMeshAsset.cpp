#include "SkeletalMeshAsset.h"

#include <algorithm>

int32 FSkeleton::FindBoneIndex(FName Name) const
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

void FSkeletalMeshLOD::CacheBounds()
{
	bBoundsValid = false;
	if (Vertices.empty()) return;

	FVector LocalMin = Vertices[0].Pos;
	FVector LocalMax = Vertices[0].Pos;
	for (const FSkinVertex& V : Vertices)
	{
		LocalMin.X = (std::min)(LocalMin.X, V.Pos.X);
		LocalMin.Y = (std::min)(LocalMin.Y, V.Pos.Y);
		LocalMin.Z = (std::min)(LocalMin.Z, V.Pos.Z);
		LocalMax.X = (std::max)(LocalMax.X, V.Pos.X);
		LocalMax.Y = (std::max)(LocalMax.Y, V.Pos.Y);
		LocalMax.Z = (std::max)(LocalMax.Z, V.Pos.Z);
	}

	BoundsCenter = (LocalMin + LocalMax) * 0.5f;
	BoundsExtent = (LocalMax - LocalMin) * 0.5f;
	bBoundsValid = true;
}

void FSkeletalMesh::Serialize(FArchive& /*Ar*/)
{
	// Skinned LOD/Bone 직렬화는 후속 단계에서 추가.
}
