#include "Core/Property/FDistributionProperty.h"

#include "Object/Object.h"
#include "SimpleJSON/json.hpp"

json::JSON FDistributionProperty::Serialize(const void* Instance) const
{
	const UObject* Value = *static_cast<UObject* const*>(ContainerPtrToValuePtr(Instance));
	return json::JSON(Value ? Value->GetName() : FString("None"));
}

void FDistributionProperty::Deserialize(void* Instance, const json::JSON& Value) const
{
}

void FDistributionProperty::SerializeItem(FArchive& Ar, void* Value, const void* Defaults) const
{
}

UObject* FDistributionProperty::GetDistributionPropertyValue(void* PropertyMemoryAddress) const
{
	return PropertyMemoryAddress ? *static_cast<UObject**>(PropertyMemoryAddress) : nullptr;
}

void FDistributionProperty::SetDistributionPropertyValue(void* PropertyMemoryAddress, UObject* Value) const
{
	if (PropertyMemoryAddress)
	{
		*static_cast<UObject**>(PropertyMemoryAddress) = Value;
	}
}
