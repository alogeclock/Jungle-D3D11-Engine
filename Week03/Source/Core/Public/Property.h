#pragma once
#include "FName.h"

enum class EPropertyType
{
    Int,
    Float,
    Bool,
    Vec3,
    Transform
};

struct FProperty
{
    FString       Name;
    EPropertyType Type;
    size_t        Offset;

    FProperty(const char* InName, EPropertyType InType, size_t InOffset) : Name(InName), Type(InType), Offset(InOffset) {}

    void *GetValuePtr(void *ObjectBase) const { return reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(ObjectBase) + Offset); }

    const void *GetValuePtr(const void *ObjectBase) const { return reinterpret_cast<const void *>(reinterpret_cast<const uint8_t *>(ObjectBase) + Offset); }
};