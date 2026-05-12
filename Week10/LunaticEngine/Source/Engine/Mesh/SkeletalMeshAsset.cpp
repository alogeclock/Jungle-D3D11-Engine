#include "SkeletalMeshAsset.h"

#include <algorithm>



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
	//TODO
}