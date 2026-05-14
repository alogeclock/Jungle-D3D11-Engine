#pragma once
#include "Core/PropertyTypes.h"	
#include "Serialization/Archive.h"

enum class EPropertyFlags : uint32 {
	CPF_Edit,			// The property can be edited in the Details Panel.
	CPF_Transient,		// The property is not saved to disk (ignored during serialization).
	CPF_Config,			// TODO: The property can be loaded from and saved to .ini configuration files.
};

class FProperty {
	const char*	   Name;
	EPropertyType  PropertyType;
	EPropertyFlags PropertyFlags;
	uint32		   ElementSize;
	uint32		   Offset_Internal;

	float Min	= 0.f;
	float Max	= 0.f;
	float Speed = 0.01f;

	// Enum Metadata
	const char** EnumNames = nullptr;
	uint32		 EnumCount = 0;
	uint32		 EnumSize  = sizeof(int32);

	// Struct Metadata
	FStructPropertyFunc StructFunc = nullptr;

	bool Identical(const void* A, const void* B);
	void Serialize(FArchive& InArchive);
};