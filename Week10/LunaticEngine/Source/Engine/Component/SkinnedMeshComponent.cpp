#include "Component/SkinnedMeshComponent.h"

#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Serialization/Archive.h"

#include <string>

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)
HIDE_FROM_COMPONENT_LIST(USkinnedMeshComponent)

namespace
{
	// 에셋/머티리얼 경로가 비어 있는 참조인지 확인합니다.
	bool IsNonePath(const FString& Path)
	{
		return Path.empty() || Path == "None";
	}

	// 비어 있는 에셋 경로를 에디터에서 쓰는 None 값으로 정규화합니다.
	void NormalizeAssetPath(FString& Path)
	{
		if (Path.empty())
		{
			Path = "None";
		}
	}

	// 비-trivial 배열 직렬화 경로를 피해서 머티리얼 슬롯을 직접 직렬화합니다.
	void SerializeMaterialSlots(FArchive& Ar, TArray<FMaterialSlot>& Slots)
	{
		uint32 SlotCount = static_cast<uint32>(Slots.size());
		Ar << SlotCount;

		if (Ar.IsLoading())
		{
			Slots.resize(SlotCount);
		}

		for (FMaterialSlot& Slot : Slots)
		{
			Ar << Slot.Path;
		}
	}
}

// 머티리얼 요소에 적용된 override 머티리얼을 반환합니다.
UMaterial* USkinnedMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex >= 0 && ElementIndex < static_cast<int32>(OverrideMaterials.size()))
	{
		return OverrideMaterials[ElementIndex];
	}
	return nullptr;
}

// 편집 가능한 머티리얼 슬롯을 반환하고, 필요하면 기본 슬롯 저장소를 만듭니다.
FMaterialSlot* USkinnedMeshComponent::GetMaterialSlot(int32 ElementIndex)
{
	EnsureMaterialSlotsForEditing();
	return (ElementIndex >= 0 && ElementIndex < static_cast<int32>(MaterialSlots.size()))
		? &MaterialSlots[ElementIndex]
		: nullptr;
}

// 슬롯 저장소를 변경하지 않고 읽기 전용 머티리얼 슬롯을 반환합니다.
const FMaterialSlot* USkinnedMeshComponent::GetMaterialSlot(int32 ElementIndex) const
{
	return (ElementIndex >= 0 && ElementIndex < static_cast<int32>(MaterialSlots.size()))
		? &MaterialSlots[ElementIndex]
		: nullptr;
}

// 에디터 표시를 위해 머티리얼 슬롯 배열 크기를 보장합니다.
void USkinnedMeshComponent::EnsureMaterialSlotsForEditing()
{
	NormalizeAssetPath(SkinnedMeshAssetPath);

	if (MaterialSlots.empty() && !IsNonePath(SkinnedMeshAssetPath))
	{
		MaterialSlots.resize(1);
		MaterialSlots[0].Path = "None";
	}

	if (OverrideMaterials.size() < MaterialSlots.size())
	{
		OverrideMaterials.resize(MaterialSlots.size(), nullptr);
	}

	for (FMaterialSlot& Slot : MaterialSlots)
	{
		NormalizeAssetPath(Slot.Path);
	}
}

// 직렬화된 머티리얼 슬롯 경로로부터 override 머티리얼 포인터를 다시 구성합니다.
void USkinnedMeshComponent::ResolveMaterialSlotsFromPaths()
{
	EnsureMaterialSlotsForEditing();
	OverrideMaterials.resize(MaterialSlots.size(), nullptr);

	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(MaterialSlots.size()); ++SlotIndex)
	{
		const FString& MatPath = MaterialSlots[SlotIndex].Path;
		OverrideMaterials[SlotIndex] = IsNonePath(MatPath)
			? nullptr
			: FMaterialManager::Get().GetOrCreateMaterial(MatPath);
	}
}

// 포즈, 스키닝, 바운드, 렌더 메시 데이터가 갱신되어야 함을 표시합니다.
void USkinnedMeshComponent::MarkSkinnedMeshDataDirty()
{
	bPoseDirty = true;
	bSkinningDirty = true;
	bBoundsDirty = true;

	MarkProxyDirty(EDirtyFlag::Mesh);
	MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::Serialize(FArchive& Ar)
{
	UMeshComponent::Serialize(Ar);

	Ar << SkinnedMeshAssetPath;
	SerializeMaterialSlots(Ar, MaterialSlots);
	Ar << bCPUSkinning;
	Ar << bDisplayBones;
	Ar << bHideSkin;

	if (Ar.IsLoading())
	{
		NormalizeAssetPath(SkinnedMeshAssetPath);
		ResolveMaterialSlotsFromPaths();
		MarkSkinnedMeshDataDirty();
	}
}

void USkinnedMeshComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	NormalizeAssetPath(SkinnedMeshAssetPath);
	ResolveMaterialSlotsFromPaths();
	MarkSkinnedMeshDataDirty();
}

void USkinnedMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureMaterialSlotsForEditing();

	UMeshComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "CPU Skinning", EPropertyType::Bool, &bCPUSkinning });
	OutProps.push_back({ "Display Bones", EPropertyType::Bool, &bDisplayBones });
	OutProps.push_back({ "Hide Skin", EPropertyType::Bool, &bHideSkin });

	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(MaterialSlots.size()); ++SlotIndex)
	{
		FPropertyDescriptor Desc;
		Desc.Name = "Element " + std::to_string(SlotIndex);
		Desc.Type = EPropertyType::MaterialSlot;
		Desc.ValuePtr = &MaterialSlots[SlotIndex];
		OutProps.push_back(Desc);
	}
}

void USkinnedMeshComponent::PostEditProperty(const char* PropertyName)
{
	UMeshComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "CPU Skinning") == 0)
	{
		bSkinningDirty = true;
		MarkProxyDirty(EDirtyFlag::Mesh);
		return;
	}

	if (std::strcmp(PropertyName, "Display Bones") == 0 || std::strcmp(PropertyName, "Hide Skin") == 0)
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
		return;
	}

	if (std::strncmp(PropertyName, "Element ", 8) == 0)
	{
		const int32 SlotIndex = std::atoi(&PropertyName[8]);
		if (SlotIndex >= 0 && SlotIndex < static_cast<int32>(MaterialSlots.size()))
		{
			NormalizeAssetPath(MaterialSlots[SlotIndex].Path);
			ResolveMaterialSlotsFromPaths();
			MarkProxyDirty(EDirtyFlag::Material);
		}
	}
}
