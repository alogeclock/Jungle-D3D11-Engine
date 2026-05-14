# Unreal-Style Property Reflection Report

Date: 2026-05-14

## Purpose

This report summarizes how UObject properties are currently represented in this codebase and what is still missing for an Unreal-style property reflection system.

The current implementation already has useful editor-facing property descriptors and UObject class registration. However, properties are not yet class-owned reflected metadata in the Unreal sense. They are mostly built manually per object through `GetEditableProperties`.

## Current Architecture

### UObject Runtime Type Information

The engine has a lightweight UObject RTTI system.

Relevant files:

- `KraftonEngine/Source/Engine/Object/Object.h`
- `KraftonEngine/Source/Engine/Object/Object.cpp`
- `KraftonEngine/Source/Engine/Object/UClass.h`
- `KraftonEngine/Source/Engine/Object/ObjectFactory.h`

Important pieces:

- `DECLARE_CLASS(ClassName, ParentClass)` declares `StaticClassInstance`, `StaticClass()`, `GetClass()`, and `Super`.
- `DEFINE_CLASS` / `DEFINE_CLASS_WITH_FLAGS` define each class's static `UClass`.
- `IMPLEMENT_CLASS` combines class definition with factory registration.
- `UClass` stores:
  - class name
  - superclass pointer
  - class size
  - class flags
- `UClass::IsA` walks the superclass chain.
- `UClass::GetAllClasses()` stores globally registered classes.
- `UClass::FindByName()` enables class lookup by string.
- `FObjectFactory` can instantiate registered UObject classes by class name.

This is enough for:

- `Cast<T>`
- `IsA`
- class-name based scene loading
- class-name based object duplication
- editor component class lists

### UObject Lifetime Tracking

`UObject` also tracks live objects globally.

Important pieces:

- `GUObjectArray`
- `GUObjectSet`
- `UUID`
- `InternalIndex`
- `Outer`
- `IsValid`
- `UObjectManager::CreateObject`
- `UObjectManager::DestroyObject`

This gives the engine a base for object identity and object iteration, but it is not property reflection by itself.

### Current Property Descriptor System

Editable properties are represented by `FPropertyDescriptor`.

Relevant files:

- `KraftonEngine/Source/Engine/Core/PropertyTypes.h`
- `KraftonEngine/Source/Engine/Core/PropertyTypes.cpp`

Current property types:

- `Bool`
- `ByteBool`
- `Int`
- `Float`
- `Vec3`
- `Vec4`
- `Rotator`
- `String`
- `Name`
- `SceneComponentRef`
- `Color4`
- `StaticMeshRef`
- `SkeletalMeshRef`
- `MaterialSlot`
- `Enum`
- `Vec3Array`
- `Struct`
- `Script`

`FPropertyDescriptor` stores:

- display/property name
- property type
- category
- raw `void* ValuePtr`
- numeric min/max/speed hints
- enum names/count/underlying size
- struct child descriptor callback

It also provides JSON serialization and deserialization for supported types.

### Manual Property Exposure

UObject exposes these virtual hooks:

- `virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)`
- `virtual void PostEditProperty(const char* PropertyName)`

Derived classes manually push descriptors into `OutProps`.

Examples:

- `AActor::GetEditableProperties`
  - exposes `Location`, `Rotation`, `Scale`, `Visible`, `Tags`
- `UActorComponent::GetEditableProperties`
  - exposes `bTickEnable`, `bEditorOnly`
- `USceneComponent::GetEditableProperties`
  - exposes relative transform properties
- `UPrimitiveComponent::GetEditableProperties`
  - exposes rendering/physics-related fields
- `UStaticMeshComponent::GetEditableProperties`
  - exposes mesh/material information
- `UCarMovementComponent::GetEditableProperties`
  - exposes vehicle movement tuning fields

I found 71 `GetEditableProperties` declarations/usages and 34 concrete implementation hits across Engine/Game source.

### Editor Details Panel

The editor property UI is descriptor-driven.

Relevant file:

- `KraftonEngine/Source/Editor/UI/EditorPropertyWidget.cpp`

Important paths:

- Actor details call `PrimaryActor->GetEditableProperties(Props)`.
- Component details call `SelectedComponent->GetEditableProperties(Props)`.
- `RenderPropertyWidget` switches on `EPropertyType` to render ImGui controls.
- After a property changes, the editor calls `PostEditProperty`.
- Multi-selection propagation copies values by matching property names and descriptor types.

This means the current descriptor system is already practically useful for the editor.

### Scene JSON Serialization

Scene save/load also reuses `GetEditableProperties`.

Relevant file:

- `KraftonEngine/Source/Engine/Serialization/SceneSaveManager.cpp`

Important paths:

- `SerializeProperties(UObject* Obj)`
  - calls `Obj->GetEditableProperties`
  - serializes each descriptor with `FPropertyDescriptor::Serialize`
- `DeserializeProperties(UObject* Obj, json::JSON& PropsJSON)`
  - calls `Obj->GetEditableProperties`
  - deserializes matching JSON values into descriptor pointers
  - calls `Obj->PostEditProperty`
  - performs a second pass because some properties appear only after earlier properties are applied

The second pass is important for dynamic property sets such as material slots created after a mesh is assigned.

### Binary Serialization and Duplication

Binary-style serialization is separate.

Relevant paths:

- `UObject::Serialize(FArchive& Ar)`
- overrides in actors, components, assets, and game classes
- `UObject::Duplicate`
- `AActor::Duplicate`

Duplication works by:

1. Creating another instance of the same class through `FObjectFactory`.
2. Serializing the source object into `FMemoryArchive`.
3. Loading that archive into the duplicate.
4. Calling `PostDuplicate`.

This path uses handwritten `Serialize(FArchive&)`, not reflected property metadata.

## What Is Already In Place

The codebase already has:

- UObject base class and object identity.
- Runtime class metadata via `UClass`.
- Class inheritance queries through `IsA`.
- Class registration through static initialization.
- Class-name based object factories.
- Global live object tracking.
- Editor-facing property descriptors.
- Property categories and simple UI hints.
- Enum and struct descriptor support in a limited form.
- JSON serialization/deserialization for editor properties.
- A descriptor-driven editor details panel.
- `PostEditProperty` hooks for applying side effects after edits.
- Handwritten binary serialization through `Serialize(FArchive&)`.
- Duplication support through factory creation plus binary serialization.

These pieces are a strong foundation.

## What Is Missing For Unreal-Style Property Reflection

### Class-Owned Property Metadata

The biggest missing piece is that `UClass` does not own reflected properties.

Current state:

- Properties are generated on demand by each object instance.
- Metadata lives temporarily in `FPropertyDescriptor` arrays.
- The only persistent class metadata is name, superclass, size, and flags.

Unreal-style target:

- `UClass` should own a list of reflected properties.
- Properties should be registered once per class.
- Property metadata should be queryable without constructing ad hoc editor descriptors.

### UPROPERTY-Style Declaration Or Registration

There is no equivalent of:

- `UPROPERTY(...)`
- generated property registration
- static property tables
- class construction metadata

Current properties are handwritten like:

```cpp
OutProps.push_back({ "MaxSpeed", EPropertyType::Float, "Movement", &MaxSpeed, 0.0f, 200.0f, 0.5f });
```

That works, but it repeats metadata manually and only exists after calling a virtual method on an instance.

### Offset-Based Access

Current descriptors store live `void* ValuePtr`.

This means:

- descriptors are tied to one object instance
- there is no reusable class-level property layout
- properties cannot be inspected independently of an instance
- property access cannot be computed generically from object base pointer plus offset

Unreal-style target:

- reflected property stores member offset
- value pointer is computed from `ObjectBase + Offset`
- the same metadata works for all instances of the class

### Property Flags And Metadata

The current descriptor has some UI hints, but no full property flag model.

Missing examples:

- editable
- visible-only
- transient
- serializable
- save-game
- duplicate-transient
- editor-only
- read-only
- asset reference kind
- object reference kind
- category metadata
- clamp min/max metadata
- display name metadata
- tooltip metadata
- hidden/advanced metadata

Some of this exists informally through `Category`, `Min`, `Max`, and `Speed`, but not as a general metadata system.

### Unified Serialization

Serialization is split between:

- handwritten `Serialize(FArchive&)`
- descriptor-based scene JSON serialization

This creates risk:

- a property can be duplicated but not scene-saved
- a property can be scene-saved but not duplicated
- a property can appear in the editor but not binary serialization
- adding a member requires updating several independent code paths

Unreal-style target:

- reflected properties can drive common serialization paths
- handwritten serialization remains possible for special cases
- flags decide which properties participate in which serialization mode

### First-Class Struct And Enum Reflection

Current support:

- enum descriptors can hold names/count/underlying size
- struct descriptors can call `StructFunc` to produce child descriptors

Missing:

- persistent `UStruct`-like metadata
- persistent `UEnum`-like metadata
- reusable reflected struct property lists
- nested property traversal through class-owned metadata
- property path support for nested edits

### Object Reference Reflection

Current asset/object references are handled case-by-case, often as strings.

Missing:

- reflected UObject pointer properties
- class-restricted object references
- asset reference metadata
- reference traversal
- object fix-up after load
- GC/reachability integration

Even if this engine does not implement full Unreal GC, property reflection would still be the natural place to discover object references.

### Rich Edit Events

Current hook:

```cpp
PostEditProperty(const char* PropertyName)
```

Missing:

- property pointer in the event
- old/new value context
- nested property chain
- array index
- change type
- transaction/undo information
- pre-change notification

The current string hook is simple and usable, but it will become limiting for arrays, structs, undo, and more precise editor behavior.

### Default Object / Class Defaults

`UClass` does not appear to own a class default object or default property data.

Missing:

- CDO-style default object
- default value comparison
- reset-to-default
- archetype-style construction
- property initialization from class defaults

This is not required for the first reflection pass, but it is central to Unreal's property model.

## Practical Assessment

The current system is not "wrong"; it is a sensible manual descriptor layer that already powers editor UI and scene JSON. The main limitation is that it is instance-driven and editor-shaped rather than class-driven and reflection-shaped.

Current model:

```text
object instance -> GetEditableProperties() -> temporary descriptors -> editor / JSON
```

Unreal-style model:

```text
class metadata -> reflected properties -> generic access for editor / serialization / duplication / scripting / references
```

## Recommended Next Steps

### 1. Introduce `FProperty` Metadata

Add a persistent reflected property type, separate from `FPropertyDescriptor`.

Suggested fields:

- name
- type
- offset
- size
- category
- flags
- metadata map or simple metadata struct
- optional enum metadata
- optional struct metadata
- optional object/class metadata

`FPropertyDescriptor` can continue to exist as an editor adapter.

### 2. Add Property Storage To `UClass`

Extend `UClass` with:

- own properties declared by that class
- inherited property iteration
- `FindPropertyByName`
- `ForEachProperty`

This turns properties into reusable class metadata.

### 3. Add Manual Registration Macros First

Before building a code generator, use explicit macros or static registration helpers.

Example direction:

```cpp
REGISTER_PROPERTY(UCarMovementComponent, MaxSpeed, EPropertyType::Float, "Movement")
```

or:

```cpp
BEGIN_CLASS_PROPERTIES(UCarMovementComponent)
    PROPERTY_FLOAT(MaxSpeed, "Movement", 0.0f, 200.0f, 0.5f)
    PROPERTY_FLOAT(AccelForce, "Movement", 0.0f, 5000.0f, 10.0f)
END_CLASS_PROPERTIES()
```

This would avoid needing a full Unreal Header Tool equivalent immediately.

### 4. Adapt Editor Descriptors From Reflected Properties

Keep `EditorPropertyWidget` mostly intact at first.

Add an adapter:

```text
UObject + FProperty metadata -> FPropertyDescriptor
```

That lets the existing editor UI continue to work while the underlying property source moves from virtual manual descriptors to class metadata.

### 5. Gradually Migrate Existing Classes

Start with simple classes:

- `UActorComponent`
- `USceneComponent`
- `AActor`
- one or two simple movement/light components

Keep custom `GetEditableProperties` support temporarily for dynamic cases like material slots.

### 6. Unify JSON Serialization Through Reflection

Once class-owned properties exist, update `SceneSaveManager` to use reflected properties directly.

Use property flags to decide:

- editor visible
- scene serializable
- transient
- editor-only

### 7. Decide How Much Unreal Fidelity Is Needed

Full Unreal reflection includes many large systems:

- UHT-generated code
- CDOs
- property flags
- metadata
- GC reference traversal
- Blueprint exposure
- replication
- config serialization
- transactions/undo

This engine likely does not need all of that immediately. A practical first milestone is:

```text
class-owned property metadata + offset access + editor/JSON integration
```

That milestone would already remove most manual duplication and create a real foundation for future systems.

## Summary

The repo currently has:

- lightweight UObject RTTI
- class registration and factory creation
- manual editable property descriptors
- descriptor-driven editor UI
- descriptor-driven scene JSON
- handwritten binary serialization

The repo is missing:

- persistent class-owned reflected property metadata
- UPROPERTY-style declaration/registration
- offset-based property access
- property flags and general metadata
- unified serialization through reflection
- first-class struct/enum/object-reference reflection
- richer edit events
- CDO/default-property support

The best next architectural step is to add `FProperty` metadata owned by `UClass`, then treat the current `FPropertyDescriptor` system as an editor compatibility layer.
