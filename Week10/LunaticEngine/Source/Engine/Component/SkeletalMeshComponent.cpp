#include "Component/SkeletalMeshComponent.h"

#include <algorithm>
#include <cmath>

#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Engine/Runtime/Engine.h"
#include "Materials/MaterialManager.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Skeletal/SkeletalMeshObject.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

// base의 unique_ptr<FSkeletalMeshObject> 소멸 시 complete type을 보기 위해 여기서 정의.
USkeletalMeshComponent::~USkeletalMeshComponent() = default;




void USkeletalMeshComponent::RefreshBoneTransforms()
{
	Super::RefreshBoneTransforms();
}
