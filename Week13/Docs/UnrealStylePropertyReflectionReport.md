# Unreal-Style Property Reflection Report

Date: 2026-05-15

## Current State

The first property-reflection milestone is now largely in place.

The engine has moved from instance-built editor descriptors toward class-owned reflected metadata:

```text
UClass property metadata -> UObject::GetEditableProperties() -> editor / JSON
```

Relevant core files:

- `KraftonEngine/Source/Engine/Object/Object.h`
- `KraftonEngine/Source/Engine/Object/UClass.h`
- `KraftonEngine/Source/Engine/Core/PropertyTypes.h`
- `KraftonEngine/Source/Engine/Core/PropertyTypes.cpp`
- `KraftonEngine/Source/Engine/Serialization/SceneSaveManager.cpp`

## What Exists Now

### Class-Owned Property Metadata

`UClass` owns `FProperty*` entries and can enumerate inherited properties through `GetAllProperties`.

`UObject::GetEditableProperties` now binds reflected templates to object instances by applying `Offset_Internal`:

```text
class metadata + object base address -> live property descriptor
```

### Registration Macros

The reflection layer currently supports:

- direct properties such as `PROPERTY_BOOL`, `PROPERTY_FLOAT`, `PROPERTY_VEC3`
- generic direct registration through `REGISTER_PROPERTY`
- explicit-offset registration for nested fields through `*_OFFSET`
- enums and structs
- generic arrays through `PROPERTY_ARRAY`

The macro surface intentionally keeps explicit offset calculation at call sites instead of multiplying `_NESTED` variants.

### Generic Arrays

`PROPERTY_ARRAY` stores the address of the `TArray<T>` object itself and uses a type-erased `FArrayAccessor` for element access.

Current array support includes:

- editor rendering
- JSON serialization / deserialization
- multi-select propagation
- `CPF_FixedSize` for arrays whose element count must not be edited by the UI

Current array users include:

- `UInterpToMovementComponent::ControlPoints`
- mesh material slots on static and skinned mesh components

### Mesh Material Migration

`UStaticMeshComponent` and `USkinnedMeshComponent` now expose material slots as reflected fixed-size arrays named `Materials`.

Legacy scene files used sibling keys:

```text
Element 0
Element 1
...
```

New scene files use:

```text
Materials: [ ... ]
```

`SceneSaveManager` currently contains a deliberately narrow compatibility helper that upgrades the old mesh-material layout during load. It is intentionally easy to remove later.

## Component Migration Status

Most engine components under `Source/Engine/Component` now register their editable state with reflection macros.

Examples already migrated:

- actor / primitive base component properties
- cameras and cine cameras
- lights
- shapes
- fog
- decals
- spring arm
- movement components
- billboards
- static and skinned mesh asset/material properties
- text render properties
- SubUV properties

## Remaining Manual Paths

These are no longer broad migration gaps; they are specific behavior cases.

### `USceneComponent`

`Location`, `Rotation`, and `Scale` are reflected, but `GetEditableProperties` remains overridden.

Reason:

- editor rotation is backed by `CachedEditRotator`
- it must be synchronized from the real quaternion before instance binding
- `PostEditProperty("Rotation")` pushes the cached Euler value back into the transform

This is a justified pre-bind hook, not a failed migration.

### `USubUVComponent`

SubUV uses reflection macros for its own properties but still overrides `GetEditableProperties`.

Reason:

- it inherits from `UBillboardComponent`
- billboard `Material` is meaningless for SubUV because SubUV builds its own material
- the override removes only the inherited `Material` descriptor after normal reflected enumeration

### `UTemporaryBoneAnimatorComponent`

This component still builds its properties manually.

Reason:

- it intentionally omits inherited `UActorComponent` editor properties
- it is also feature-gated by `JUNGLE_ENABLE_TEMP_BONE_ANIMATOR_COMPONENT`

This is the one remaining truly manual descriptor block in the component directory.

## Review Findings

### 1. `UTextRenderComponent` Has A Parent-Class Mismatch

The C++ inheritance and implementation use `UBillboardComponent`:

```cpp
class UTextRenderComponent : public UBillboardComponent
IMPLEMENT_CLASS(UTextRenderComponent, UBillboardComponent)
```

but the header still declares:

```cpp
DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)
```

That makes the generated `Super` alias inconsistent with the real base class and with RTTI registration. This should be reconciled deliberately.

### 2. `UTextRenderComponent::PostEditProperty` Still Skips Primitive Handling

The current override calls `USceneComponent::PostEditProperty`.

Because text render now participates in reflected primitive properties, the safer base call is probably:

```cpp
UPrimitiveComponent::PostEditProperty(PropertyName);
```

Otherwise inherited primitive edits such as visibility / shadow / collision behavior can bypass primitive-side post-edit handling.

### 3. Text Render And SubUV Now Need A Clear Inheritance Policy

Both inherit from `UBillboardComponent`, but they do not necessarily want every billboard property.

Current solutions are ad hoc:

- `USubUVComponent` removes inherited `Material`
- `UTextRenderComponent` currently inherits according to its runtime class chain, while older code comments suggest it previously wanted to skip billboard properties

The next reflection feature should decide this centrally instead of relying on per-class filtering.

## Recommended Next Steps

### 1. Add Property-Level Visibility / Inheritance Control

The current system has `CPF_Edit`, `CPF_FixedSize`, and serialization-related flags, but it does not yet have a clean way for a derived class to suppress an inherited editable property.

Likely next feature:

- a hidden/edit-suppression flag
- or class-level override metadata for inherited properties
- or a reflected-property filter hook separate from full manual descriptor construction

This would cleanly cover:

- `USubUVComponent` hiding billboard `Material`
- any future component that wants storage inheritance without editor inheritance

### 2. Separate "All Reflected" From "Editable"

`GetEditableProperties` still acts as the main bridge for editor rendering and scene JSON.

The next larger architectural step should split:

- all reflected properties
- editable properties
- serializable properties

That would let flags such as `CPF_Edit`, `CPF_Transient`, and future save-related flags become authoritative rather than advisory.

### 3. Finish Serializer Migration Deliberately

Scene JSON now uses reflection heavily, but handwritten `Serialize(FArchive&)` still exists throughout the engine.

Recommended order:

1. define the intended save flags
2. make JSON serialization filter by flags
3. decide which binary/archive paths should stay handwritten
4. only then broaden reflection-driven serialization

### 4. Add Focused Regression Coverage

Useful first tests:

- inherited property ordering
- nested offset binding
- array serialize / deserialize
- fixed-size array preservation
- legacy mesh-material upgrade
- `PostEditProperty` dispatch for reflected arrays and nested values

### 5. Consider Defaults / CDO-Like Behavior Later

Still missing from an Unreal-style model:

- class defaults
- reset-to-default
- default-value comparison
- object-reference traversal
- richer metadata
- transaction / undo integration

These are valuable, but the present codebase is better served by stabilizing the current reflected-property layer first.

## Summary

The reflection-macro migration is effectively complete for normal engine components.

The codebase now has:

- class-owned `FProperty` metadata
- offset-based instance binding
- direct, offset, enum, struct, and array macros
- generic reflected arrays with fixed-size support
- reflected mesh material arrays plus explicit legacy scene compatibility

The next phase is no longer "migrate more simple properties." It is to make inheritance, visibility, serialization flags, and a few remaining special cases more principled.
