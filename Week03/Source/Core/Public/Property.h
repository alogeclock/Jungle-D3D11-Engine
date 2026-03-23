#pragma once
#include "FName.h"

enum class EPropertyType
{
    UObjectPtr,
    UObjectPtrArray,
    UObject,
    UObjectDetail,
    Int,
    Float,
    Bool,
    Vec3,
    Transform,
    String,
};

struct FProperty
{
    FString       Name;
    EPropertyType Type;
    size_t        Offset;
    bool          bIsPublic;

    FProperty(FString InName, EPropertyType InType, size_t InOffset, bool IsPublic) : Name(InName), Type(InType), Offset(InOffset), bIsPublic(IsPublic) {}

    void *GetValuePtr(void *ObjectBase) const { return reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(ObjectBase) + Offset); }

    const void *GetValuePtr(const void *ObjectBase) const { return reinterpret_cast<const void *>(reinterpret_cast<const uint8_t *>(ObjectBase) + Offset); }
};