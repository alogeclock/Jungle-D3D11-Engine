#include "UClass.h"
#include "Serialization/Archive.h"

//void UClass::Serialize(FArchive& Ar)
//{
//	for (uint32 i = 0; i < Properties.size(); i++)
//	{
//		FProperty* Property = Properties[i];
//		if (!Property || (Property->PropertyFlag & EPropertyFlags::CPF_Transient) != 0) continue;
//		Ar << Property->ValuePtr;
//	}
//}

void UClass::HideInheritedProperty(FString InName)
{
	HiddenProperties.insert(InName);
}

bool UClass::IsPropertyHidden(FString InName) const
{
	return HiddenProperties.contains(InName);
}

void UClass::GetEditableProperties(TArray<FProperty>& OutProps) const
{
	GetEditablePropertiesFor(OutProps, this);
}

void UClass::GetNonTransientProperties(TArray<FProperty>& OutProps) const
{
	GetNonTransientPropertiesFor(OutProps, this);
}

void UClass::GetEditablePropertiesFor(TArray<FProperty>& OutProps, const UClass* TargetClass) const
{
	if (SuperClass) SuperClass->GetEditablePropertiesFor(OutProps, TargetClass);

	for (uint32 i = 0; i < Properties.size(); i++)
	{
		FProperty* Property = Properties[i];
		if (!Property || TargetClass->IsPropertyHidden(Property->Name)) continue;
		if (Property->PropertyFlag & EPropertyFlags::CPF_Edit) OutProps.push_back(*Property);
	}
}

void UClass::GetNonTransientPropertiesFor(TArray<FProperty>& OutProps, const UClass* TargetClass) const
{
	if (SuperClass) SuperClass->GetNonTransientPropertiesFor(OutProps, TargetClass);

	for (uint32 i = 0; i < Properties.size(); i++)
	{
		FProperty* Property = Properties[i];
		if (!Property || TargetClass->IsPropertyHidden(Property->Name)) continue;
		if ((Property->PropertyFlag & EPropertyFlags::CPF_Transient) == 0) OutProps.push_back(*Property);
	}
}