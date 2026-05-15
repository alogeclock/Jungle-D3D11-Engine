#include "Component/Light/LightComponent.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ULightComponent, ULightComponentBase)
HIDE_FROM_COMPONENT_LIST(ULightComponent)

BEGIN_CLASS_PROPERTIES(ULightComponent)
	PROPERTY_FLOAT(ShadowResolutionScale, "Shadow Resolution Scale", "Shadow", 0.1f, 4.0f, 0.1f, EPropertyFlags::CPF_Edit)
	PROPERTY_FLOAT(ShadowBias, "Shadow Bias", "Shadow", -0.2f, 0.2f, 0.0001f, EPropertyFlags::CPF_Edit)
	PROPERTY_FLOAT(ShadowSlopeBias, "Shadow Slope Bias", "Shadow", -0.2f, 0.2f, 0.0001f, EPropertyFlags::CPF_Edit)
	PROPERTY_FLOAT(ShadowNormalBias, "Shadow Normal Bias", "Shadow", -0.2f, 0.2f, 0.0001f, EPropertyFlags::CPF_Edit)
	PROPERTY_FLOAT(ShadowSharpen, "Shadow Sharpen", "Shadow", 0.f, 1.f, 0.06f, EPropertyFlags::CPF_Edit)
END_CLASS_PROPERTIES(ULightComponent)

void ULightComponent::Serialize(FArchive& Ar)
{
	ULightComponentBase::Serialize(Ar);
	Ar << ShadowResolutionScale;
	Ar << ShadowBias;
	Ar << ShadowSlopeBias;
	Ar << ShadowNormalBias;
	Ar << ShadowSharpen;
}

//void ULightComponent::GetEditableProperties(TArray<FProperty>& OutProps)
//{
//	ULightComponentBase::GetEditableProperties(OutProps);
//	OutProps.push_back({ "Shadow Resolution Scale", EPropertyType::Float, "Shadow", &ShadowResolutionScale, 0.1f, 4.0f, 0.1f });
//	OutProps.push_back({ "Shadow Bias",             EPropertyType::Float, "Shadow", &ShadowBias,            -0.2f, 0.2f, 0.0001f });
//	OutProps.push_back({ "Shadow Slope Bias",       EPropertyType::Float, "Shadow", &ShadowSlopeBias,       -0.2f, 0.2f, 0.0001f });
//	OutProps.push_back({ "Shadow Normal Bias",      EPropertyType::Float, "Shadow", &ShadowNormalBias,      -0.2f, 0.2f, 0.0001f });
//	OutProps.push_back({ "Shadow Sharpen",          EPropertyType::Float, "Shadow", &ShadowSharpen,         0.0f, 1.0f, 0.05f });
//}
