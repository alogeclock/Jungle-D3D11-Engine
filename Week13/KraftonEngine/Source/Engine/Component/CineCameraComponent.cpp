#include "Component/CineCameraComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UCineCameraComponent, UCameraComponent)

BEGIN_CLASS_PROPERTIES(UCineCameraComponent)
	PROPERTY_BOOL_OFFSET("Enable Letterbox", "Cinematic", offsetof(ThisClass, Letterbox) + offsetof(FCineLetterboxSettings, bEnabled), CPF_Edit)
	PROPERTY_FLOAT_OFFSET("Letterbox Amount", "Cinematic", offsetof(ThisClass, Letterbox) + offsetof(FCineLetterboxSettings, Amount), 0.0f, 1.0f, 0.01f, CPF_Edit)
	PROPERTY_FLOAT_OFFSET("Letterbox Thickness", "Cinematic", offsetof(ThisClass, Letterbox) + offsetof(FCineLetterboxSettings, Thickness), 0.0f, 0.5f, 0.01f, CPF_Edit)
	REGISTER_PROPERTY_OFFSET("Letterbox Color", EPropertyType::Color4, "Cinematic", offsetof(ThisClass, Letterbox) + offsetof(FCineLetterboxSettings, Color), sizeof(((FCineLetterboxSettings*)0)->Color), CPF_Edit)
END_CLASS_PROPERTIES(UCineCameraComponent)

void UCineCameraComponent::Serialize(FArchive& Ar)
{
	UCameraComponent::Serialize(Ar);
	Ar << Letterbox.bEnabled;
	Ar << Letterbox.Amount;
	Ar << Letterbox.Thickness;
	Ar << Letterbox.Color;
}

