#pragma once

#include "Core/Property/PropertyTypes.h"

class UClass;
class UObject;

class FDistributionProperty final : public FProperty
{
public:
	FDistributionProperty(
		const FString& InName,
		const FString& InCategory,
		uint32 InPropertyFlag,
		uint32 InOffset,
		uint32 InElementSize,
		UClass* InPropertyClass)
		: FProperty(InName, InCategory, InPropertyFlag, InOffset, InElementSize)
		, PropertyClass(InPropertyClass)
	{
	}

	EPropertyType GetType() const override { return EPropertyType::Distribution; }
	json::JSON Serialize(const void* Instance) const override;
	void Deserialize(void* Instance, const json::JSON& Value) const override;
	void SerializeItem(FArchive& Ar, void* Value, void const* Defaults) const override;

	UObject* GetDistributionPropertyValue(void* PropertyMemoryAddress) const;
	void SetDistributionPropertyValue(void* PropertyMemoryAddress, UObject* Value) const;

	UClass* PropertyClass = nullptr;
};
