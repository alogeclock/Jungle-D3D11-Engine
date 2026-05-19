# 특수 프로퍼티 타입 처리 상세

[`PropertyReflectionSystem.md`](PropertyReflectionSystem.md) 가 리플렉션 시스템의 큰 그림을 다뤘다면, 본 문서는 **기본 값 타입(`bool`, `float`, `FVector` 등)을 넘어서는 "특수" 프로퍼티 타입들이 실제로 어떻게 처리되는지**를 더 깊게 들여다본다.

다루는 타입:

1. **`FEnumProperty`** — `enum class` 멤버
2. **`FStructProperty`** — `USTRUCT()` 로 마킹된 구조체 멤버
3. **`FArrayProperty`** — `TArray<T>` 멤버
4. **`FSceneComponentRefProperty`** — `USceneComponent*` (문자열 경로 기반 특수 케이스)
5. **`FSoftObjectProperty`** — 자산에 대한 약/지연 참조
6. **`FObjectProperty` / `FClassProperty`** — 하드 `UObject*` / `UClass*` 참조 *(현재 빈 스텁; 의도된 동작을 §6 에서 설계 수준까지 기술)*

각 항목마다 **저장 표현 → codegen 산출물 → 직렬화 → 에디터 UI → 한계와 주의점** 순으로 정리한다.

---

## 0. 공통 모델 한 번 더 짚기

모든 프로퍼티 디스크립터는 `FProperty` 의 서브클래스이며 **순수 스키마**이다. 값은 객체 메모리 안에 살고, 디스크립터는 `Offset_Internal` 로 그 주소만 계산한다:

```cpp
void* ContainerPtrToValuePtr(void* Container) const {
    return static_cast<char*>(Container) + Offset_Internal;
}
```

각 디스크립터의 가상 메서드는 둘이다:

```cpp
virtual json::JSON Serialize(const void* Instance) const;
virtual void       Deserialize(void* Instance, const json::JSON& Value) const;
```

특수 타입들이 흥미로운 이유는, 이 두 메서드 하나만으로는 부족해서 **추가 메타데이터(이름 테이블, 자식 스키마, 컨테이너 액세서, 타입 제약 클래스 등)** 를 디스크립터에 들고 있어야 하기 때문이다.

---

## 1. `FEnumProperty` — `enum class` 멤버

### 1-1. 저장 표현
저자 코드는 그냥 평범한 `enum class` 이다.

```cpp
// InterpToMovementComponent.h
UENUM()
enum class EInterpBehaviour {
    OneShot,
    OneShotReverse,
    Loop,
    PingPong
};

class UInterpToMovementComponent : public UMovementComponent
{
    UPROPERTY(Edit, Category="Movement", DisplayName="Interpolation Behaviour")
    EInterpBehaviour InterpBehaviour = EInterpBehaviour::OneShot;
};
```

저장은 enum 의 underlying type 크기 그대로(보통 4바이트). 디스크립터는 그 사이즈만 알면 된다.

### 1-2. 디스크립터 — 이제는 메타 객체 `UEnum` 을 들고 있다
[`FEnumProperty`](../KraftonEngine/Source/Engine/Core/Property/FEnumProperty.h) 는 raw 배열·정수 세 개 대신, **`UEnum* EnumDesc` 하나**만 보유한다.

```cpp
class FEnumProperty final : public FProperty {
public:
    UEnum* GetEnum() const { return EnumDesc; }

    EPropertyType GetType() const override { return EPropertyType::Enum; }
    json::JSON Serialize(const void* Instance) const override;
    void Deserialize(void* Instance, const json::JSON& Value) const override;

private:
    UEnum* EnumDesc = nullptr;
};
```

[`UEnum`](../KraftonEngine/Source/Engine/Object/UEnum.h) 은 **`UField → UObject` 체인을 정식으로 상속하는 메타 객체**이며, 다음을 제공한다.

```cpp
class UEnum : public UField {
public:
    UEnum(const char* InName,
          uint32 InUnderlyingSize = sizeof(int32),
          ECppForm InForm = ECppForm::EnumClass);

    void   AddEnumerator(const char* InName, int64 InValue);
    uint32 NumEnums() const;
    const char* GetNameByIndex(uint32 Index) const;
    int64       GetValueByIndex(uint32 Index) const;
    FString     GetNameByValue(int64 Value) const;
    int32       GetIndexByValue(int64 Value) const;
    int64       GetValueByName(FString Name) const;
    int64       GetMaxEnumValue() const;
    uint32      GetUnderlyingSize() const;

private:
    TArray<TPair<FString, int64>> DisplayNames;  // (이름, 값) 쌍 — 값은 임의의 int64
    FString  CppType;
    uint32   UnderlyingSize = sizeof(int32);
    ECppForm CppForm;   // Regular / NameSpaced / EnumClass
};
```

이전과의 핵심 차이:

| 항목 | 이전 (raw) | 현재 (`UEnum`) |
|---|---|---|
| 이름 보관 | `const char**` 정적 배열 | `TArray<TPair<FString, int64>>` |
| 항목 값 | **인덱스 = 값** 으로 암묵 가정 | 항목마다 명시적 `int64` 값 |
| 폭 정보 | `uint32 EnumSize` 필드 | `UEnum::GetUnderlyingSize()` |
| 메타 객체 위치 | 없음 (자유 배열) | `GUObjectArray` 안의 정적 객체 — RTTI 가능, `IsA<UEnum>` 가능 |
| 표현 형태 | 단일 (배열) | `ECppForm` 으로 `enum class` / namespaced / regular 구분 |

**`DisplayNames` 가 `(FString, int64)` 쌍의 배열**이라는 점이 본질적이다. 이로써 `enum class EFoo : uint8 { A = 5, B = 10, C = 200 }` 같이 **값이 sparse 하거나 큰 폭의 underlying type** 도 표현 가능하다.

### 1-3. Codegen 산출물
[`Scripts/GenerateCode.py`](../Scripts/GenerateCode.py) 는 `UENUM()` 을 발견하면 **`.generated.h` 에 자유 함수 한 개를 선언**하고 (또는 정의를 inline 으로 두고), `.gen.cpp` 에서 `UEnum` 정적 인스턴스를 만든다.

```cpp
// InterpToMovementComponent.gen.cpp
UEnum* StaticEnum_EInterpBehaviour()
{
    static UEnum Enum("EInterpBehaviour",
                      sizeof(EInterpBehaviour),
                      ECppForm::EnumClass);
    static const bool bRegistered = []()
    {
        Enum.AddEnumerator("OneShot",        static_cast<int64>(EInterpBehaviour::OneShot));
        Enum.AddEnumerator("OneShotReverse", static_cast<int64>(EInterpBehaviour::OneShotReverse));
        Enum.AddEnumerator("Loop",           static_cast<int64>(EInterpBehaviour::Loop));
        Enum.AddEnumerator("PingPong",       static_cast<int64>(EInterpBehaviour::PingPong));
        return true;
    }();
    (void)bRegistered;
    return &Enum;
}
```

- **`static UEnum`** 으로 정적 수명을 보장하고, 람다-1회-실행 트릭으로 항목 등록이 **딱 한 번** 일어나도록 만든다.
- `UEnum` 의 생성자는 부모 `UField` 의 ctor 안에서 `FUObjectArray::DeferStaticObject(this)` 를 호출하므로, EngineLoop 가 큐를 비울 때 자동으로 `GUObjectArray` 에 등록된다. (UClass · UScriptStruct 와 같은 흐름.)
- 캐스팅용 `CASTCLASS_UEnum` 비트가 정의되어 있어, 일반 `UObject*` 로부터 `IsA<UEnum>()` / `Cast<UEnum>` 가 빠르게 동작한다.

호스트 클래스의 `.gen.cpp` 의 등록 코드도 더 짧아졌다:

```cpp
Cls->AddProperty(new FEnumProperty(
    "Interpolation Behaviour", "Movement", CPF_Edit,
    offsetof(ThisClass, InterpBehaviour),
    sizeof(((ThisClass*)0)->InterpBehaviour),
    StaticEnum_EInterpBehaviour()));   // ← UEnum* 하나만 전달
```

### 1-4. 직렬화
폭 가변 대응을 위해 내부 임시 변수가 `int64` 로 확장되었다.

```cpp
// PropertyTypes.cpp
json::JSON FEnumProperty::Serialize(const void* Instance) const {
    const void* ValuePtr = ContainerPtrToValuePtr(Instance);
    int64 Value = 0;
    const uint32 ValueSize = EnumDesc ? EnumDesc->GetUnderlyingSize() : ElementSize;
    std::memcpy(&Value, ValuePtr, std::min<uint32>(ValueSize, sizeof(Value)));
    return json::JSON(static_cast<int32>(Value));
}

void FEnumProperty::Deserialize(void* Instance, const json::JSON& Value) const {
    void* Target = ContainerPtrToValuePtr(Instance);
    int64 StoredValue = Value.ToInt();
    const uint32 ValueSize = EnumDesc ? EnumDesc->GetUnderlyingSize() : ElementSize;
    std::memcpy(Target, &StoredValue, std::min<uint32>(ValueSize, sizeof(StoredValue)));
}
```

요점:

- 메모리 폭은 **`EnumDesc->GetUnderlyingSize()`** 가 권위 — `EnumDesc` 가 nullptr 인 fallback 에 한해서만 디스크립터의 `ElementSize` 를 사용.
- `std::min<uint32>(ValueSize, sizeof(Value))` 클램프로 **8바이트보다 큰 가짜 underlying type 이 들어와도 스택 오버런이 나지 않음** 을 보장.
- 저장은 여전히 raw 정수 값이지만, 이제는 **인덱스가 아니라 enum 본래의 값** 이다. `enum class EFoo { A = 0, B = 10 }` 처럼 sparse 한 값도 정확히 보존된다.
- ⚠️ 최종 JSON 으로 내려갈 때 `static_cast<int32>(Value)` 로 **int32 로 다운캐스트**된다. underlying type 이 `int64` 이고 실제 값이 `INT32_MAX` 를 넘는 경우 손실이 생긴다 — 현재는 일어날 일이 드물지만 향후 64bit 보존이 필요해지면 JSON 측 폭을 늘려야 한다.

### 1-5. 에디터 UI
콤보 박스 구성도 `UEnum` 메서드 호출로 바뀐다.

```cpp
const FEnumProperty& EnumProp = static_cast<const FEnumProperty&>(Prop);
UEnum* Enum = EnumProp.GetEnum();
if (!Enum) break;

int64 Val = 0;
memcpy(&Val, ValuePtr, Enum->GetUnderlyingSize());

const char* Preview = Enum->GetNameByValue(Val).c_str();
if (ImGui::BeginCombo("##Value", Preview))
{
    for (uint32 i = 0; i < Enum->NumEnums(); ++i)
    {
        const char* Name  = Enum->GetNameByIndex(i);
        const int64 Value = Enum->GetValueByIndex(i);
        bool bSelected = (Val == Value);
        if (ImGui::Selectable(Name, bSelected))
        {
            int64 NewVal = Value;
            memcpy(ValuePtr, &NewVal, Enum->GetUnderlyingSize());
        }
    }
    ImGui::EndCombo();
}
```

> 실제 [`EditorPropertyWidget.cpp:1564`](../KraftonEngine/Source/Editor/UI/EditorPropertyWidget.cpp#L1564) 의 코드는 위 형태로 마이그레이션이 필요하다. (이전의 `EnumNames`/`EnumCount`/`EnumSize` 접근은 더 이상 컴파일되지 않는다.)

### 1-6. 한계
- 여전히 **값 기반** 직렬화 — 이름 기반이 아니다. enum 항목의 *이름* 만 바뀌고 값은 그대로면 호환되지만, 값이 바뀌면 깨진다. 다행히 이제 `UEnum` 이 이름 ↔ 값 양방향 변환을 보유하므로, 향후 "이름으로 저장" 으로의 전환은 `Serialize` / `Deserialize` 두 함수 수정만으로 끝난다.
- JSON 측 int32 다운캐스트로 **64bit 손실 가능성** (위 §1-4 참고).
- `enum` (class 아님) 은 codegen 이 `enum class` 만 파싱하므로 미지원.
- "Hidden enum entry" / "Display name override" 같은 항목별 메타데이터는 아직 `UEnum` 이 들고 있지 않다.

---

## 2. `FStructProperty` — `USTRUCT()` 구조체 멤버

### 2-1. 저장 표현
저자 코드는 그냥 POD 구조체이다.

```cpp
// Transform.h
USTRUCT()
struct FTransform
{
    GENERATED_BODY(FTransform)

    UPROPERTY(Edit, Category="Transform")
    FVector Location;

    FQuat Rotation;  // UPROPERTY 미부착 → 리플렉션 제외

    UPROPERTY(Edit, Category="Transform")
    FVector Scale;
};
```

값은 구조체 그 자체로 메모리에 인라인된다. 별도 포인터·박싱 없음.

### 2-2. 디스크립터 — 이제는 정식 메타 객체 `UScriptStruct` 를 들고 있다
[`FStructProperty`](../KraftonEngine/Source/Engine/Core/Property/FStructProperty.h) 는 자식 스키마를 *자유 함수 포인터* 로 받던 방식에서, **`UScriptStruct*` 메타 객체 하나** 를 들고 있는 형태로 진화했다.

```cpp
class FStructProperty final : public FProperty {
public:
    UScriptStruct* ScriptStruct = nullptr;

    EPropertyType GetType() const override { return EPropertyType::Struct; }
    json::JSON Serialize(const void* Instance) const override;
    void Deserialize(void* Instance, const json::JSON& Value) const override;

    const TArray<FProperty*>& GetStructProperties() const
    {
        if (ScriptStruct) return ScriptStruct->GetProperties();
        static const TArray<FProperty*> Empty;
        return Empty;
    }
};
```

자식 프로퍼티 목록은 `UStruct::GetProperties()` (즉, `UScriptStruct` 가 `UStruct` 로부터 그대로 상속) 에서 얻는다. **이전의 `SchemaFn()` 함수 포인터는 사라졌다.**

### 2-3. 메타 객체 — [`UScriptStruct`](../KraftonEngine/Source/Engine/Object/ScriptStruct.h)
WIP 였던 `UScriptStruct` 가 정식으로 들어왔다. 핵심 두 부분:

**(a) UObject 메타 계층의 정식 멤버**

```
UObject → UField → UStruct → UScriptStruct
                          ↘ UClass
```

- `UStruct` 의 `Properties` 컨테이너와 `FindPropertyByName` / `GetEditableProperties` / `GetNonTransientProperties` 가 그대로 재사용된다.
- `CASTCLASS_UScriptStruct` 비트가 있어 `IsA<UScriptStruct>` / `Cast<UScriptStruct>` 가 빠르게 동작.
- 생성자에서 `UField::UField` 가 `FUObjectArray::DeferStaticObject(this)` 를 호출하므로, `GUObjectArray` 에 자동으로 정적 객체로 등록된다 — `UEnum` / `UClass` 와 같은 흐름.

**(b) C++ 측 생애 주기 추상화 — `ICppStructOps`**

구조체가 진짜 메타 객체가 되었다는 것은, **메타 객체가 그 타입을 직접 만들고 파괴하고 복사할 수 있어야 한다** 는 뜻이다. (예: TArray 의 inner 가 USTRUCT 일 때 element 를 in-place 로 만들고 지우려면 타입 소거된 생성/파괴 인터페이스가 필요하다.)

```cpp
class ICppStructOps {
public:
    virtual void Construct(void* Dest) const = 0;
    virtual void Destruct (void* Dest) const = 0;
    virtual void Copy     (void* Dest, const void* Src) const = 0;
};

template <typename T>
class TCppStructOps final : public ICppStructOps {
public:
    void Construct(void* Dest) const override            { new (Dest) T(); }
    void Destruct (void* Dest) const override            { static_cast<T*>(Dest)->~T(); }
    void Copy     (void* Dest, const void* Src) const override
                                                         { *static_cast<T*>(Dest) = *static_cast<const T*>(Src); }
};

class UScriptStruct : public UStruct {
public:
    UScriptStruct(const char* InName, UScriptStruct* InSuperStruct,
                  size_t InSize, size_t InAlignment,
                  const ICppStructOps* InCppStructOps);

    size_t GetAlignment() const;
    const ICppStructOps* GetCppStructOps() const;

    void InitializeStruct (void* Dest) const;            // CppStructOps->Construct
    void DestroyStruct    (void* Dest) const;            // CppStructOps->Destruct
    void CopyScriptStruct (void* Dest, const void* Src) const;  // CppStructOps->Copy
};
```

이전의 `FStructPropertySchemaFn` (자유 함수 포인터) 가 갖지 못했던 능력들이 추가된다.

- **정렬(`alignof`) 정보** — TArray\<FStruct\> 같은 컨테이너가 정확한 정렬로 슬롯을 잡을 수 있음.
- **타입 소거된 생성/파괴/복사** — 향후 GC 의 회수, `TArray<FStruct>::AddDefault`, 객체 `Duplicate` 의 깊은 복사가 한 인터페이스로 처리됨.
- **상속 가능** — 생성자가 `UScriptStruct* InSuperStruct` 를 받는다. (실제 사용은 향후 작업이지만 인터페이스는 열려 있다.)

### 2-4. Codegen 산출물
`USTRUCT()` 가 붙은 타입에 대해 codegen 은 두 가지를 emit 한다.

**(a) `.generated.h`** — `KE_GENERATED_BODY_<Name>()` 매크로 안에 `StaticStruct()` 등을 선언.

**(b) `.gen.cpp`** — `TCppStructOps<T>` 인스턴스 + `UScriptStruct` 정적 인스턴스 + 자식 프로퍼티 등록 (`FTransform` 의 산출물, [`Transform.gen.cpp`](../KraftonEngine/Intermediate/Generated/Source/Transform.gen.cpp)):

```cpp
static const TCppStructOps<FTransform> GFTransformCppStructOps;

UScriptStruct FTransform::StaticStructInstance(
    "FTransform", nullptr,
    sizeof(FTransform), alignof(FTransform),
    &GFTransformCppStructOps);

static void RegisterFTransformStructProperties(UScriptStruct* Struct)
{
    static bool bRegistered = false;
    if (bRegistered || !Struct) return;
    bRegistered = true;
    Struct->AddProperty(new FVec3Property(
        "Location", "Transform", CPF_Edit,
        offsetof(FTransform, Location), sizeof(((FTransform*)0)->Location)));
    Struct->AddProperty(new FVec3Property(
        "Scale", "Transform", CPF_Edit,
        offsetof(FTransform, Scale), sizeof(((FTransform*)0)->Scale)));
}

struct FTransform_StructPropertyRegistrar {
    FTransform_StructPropertyRegistrar() {
        RegisterFTransformStructProperties(FTransform::StaticStruct());
    }
};
static FTransform_StructPropertyRegistrar s_FTransform_StructPropertyReg;
```

주목할 점:

- `TCppStructOps<FTransform>` 가 **`static const`** 로 잡혀 있어 메모리·정적 수명 모두 안정. `UScriptStruct` 는 이 인터페이스 포인터만 보유.
- 프로퍼티 등록은 별도 함수 + 1회 가드(`static bool bRegistered`) 로 구성 — `GUObjectArray` 등록과 자식 프로퍼티 등록이 한 곳에서 꼬이지 않게 분리.
- `UScriptStruct` 의 ctor 가 `UField` ctor 를 거치면서 `DeferStaticObject` 가 호출되므로, **`GUObjectArray` 등록 시점은 EngineLoop 가 큐를 비우는 안전한 시점으로 미뤄진다** ([`UField.h`](../KraftonEngine/Source/Engine/Object/UField.h) 코멘트 참고). 이전의 자유 함수 방식보다 정적 초기화 순서에 더 강건.

호스트 클래스 측 등록은 단순해졌다:

```cpp
new FStructProperty(
    "Transform", "Actor", CPF_Edit,
    offsetof(AActor, ActorTransform),
    sizeof(((AActor*)0)->ActorTransform),
    FTransform::StaticStruct());   // ← UScriptStruct* 하나만 전달
```

### 2-5. 직렬화
구현은 본질적으로 같지만, 자식 목록을 얻는 경로가 함수 포인터에서 메타 객체 호출로 바뀌었다.

```cpp
json::JSON FStructProperty::Serialize(const void* Instance) const {
    const void* StructInstance = ContainerPtrToValuePtr(Instance);
    if (!StructInstance) return json::JSON();

    const TArray<FProperty*>& StructProperties = GetStructProperties();
    if (StructProperties.empty()) return json::JSON();   // ScriptStruct nullptr 또는 빈 스키마

    json::JSON Object = json::Object();
    for (const FProperty* Child : StructProperties) {
        if (Child) Object[Child->Name] = Child->Serialize(StructInstance);
    }
    return Object;
}

void FStructProperty::Deserialize(void* Instance, const json::JSON& Value) const {
    void* StructInstance = ContainerPtrToValuePtr(Instance);
    if (!StructInstance) return;

    const TArray<FProperty*>& StructProperties = GetStructProperties();
    for (const FProperty* Child : StructProperties) {
        if (!Child || !Value.hasKey(Child->Name.c_str())) continue;
        Child->Deserialize(StructInstance, Value.at(Child->Name.c_str()));
    }
}
```

`Deserialize` 의 *"자식 키가 JSON 에 없으면 건너뛴다"* 정책은 그대로 유지되므로 **구조체에 필드를 추가해도 기존 씬 파일이 깨지지 않는다** 는 보장 역시 그대로다.

### 2-6. 에디터 UI
[`EditorPropertyWidget.cpp:1666`](../KraftonEngine/Source/Editor/UI/EditorPropertyWidget.cpp#L1666) 의 트리 노드 + 재귀 렌더 로직은 골격이 같지만, 자식 목록 접근이 `StructProp.SchemaFn()` 에서 `StructProp.GetStructProperties()` (또는 `StructProp.ScriptStruct->GetProperties()`) 로 바뀌어야 한다.

### 2-7. 한계와 향후
- **상속 인터페이스는 열려 있지만 미사용** — `UScriptStruct` ctor 가 `SuperStruct` 인자를 받기 시작했고 `UStruct::GetAllProperties` 가 부모 → 자식 순서로 재귀하므로 *기술적으로는* 가능하다. codegen 측에서 `USTRUCT()` 의 베이스를 파싱하는 작업이 추가되면 즉시 켤 수 있다.
- **`TArray<USTRUCT>`** — v1 에서 차단되어 있다 ([`GenerateCode.py:660`](../Scripts/GenerateCode.py#L660)). 이제 `UScriptStruct` 가 `ICppStructOps::Construct/Destruct/Copy` 를 제공하므로, `FArrayAccessor::AddDefault` 가 `Ops->Construct` 를 호출하도록 일반화하면 차단을 푸는 길이 열렸다.
- **GC 통합** — `UScriptStruct` 는 이제 `GUObjectArray` 의 정적 객체이므로 *메타 객체 자체* 는 자연스레 GC 루트가 된다. 구조체 *인스턴스* 내부의 `UObject*` 멤버 순회는 `FObjectProperty` 가 도입되면 다른 컨테이너와 동일한 방식으로 처리된다.

---

## 3. `FArrayProperty` — `TArray<T>`

### 3-1. 저장 표현
표준 `TArray<T>` (= `std::vector<T>` 의 typedef). 메모리는 컨테이너가 관리한다.

### 3-2. 디스크립터 + 타입 소거 액세서
[`FArrayProperty`](../KraftonEngine/Source/Engine/Core/Property/FArrayProperty.h) 의 핵심은 **T 를 모른 채로 모든 배열을 다루기 위한 함수 포인터 테이블** 이다.

```cpp
// PropertyTypes.h
struct FArrayAccessor {
    uint32 (*Num)(const void*);
    void*  (*GetAt)(void*, uint32);
    void   (*AddDefault)(void*);
    void   (*RemoveAt)(void*, uint32);
    void   (*Clear)(void*);
    void   (*Assign)(void* Dst, const void* Src);
};

template <typename T>
inline FArrayAccessor* GetTArrayAccessor() {
    static FArrayAccessor s = {
        +[](const void* A) -> uint32 { return (uint32)static_cast<const TArray<T>*>(A)->size(); },
        +[](void* A, uint32 i) -> void* { return &(*static_cast<TArray<T>*>(A))[i]; },
        +[](void* A) { static_cast<TArray<T>*>(A)->emplace_back(); },
        +[](void* A, uint32 i) { auto& V = *static_cast<TArray<T>*>(A); V.erase(V.begin() + i); },
        +[](void* A) { static_cast<TArray<T>*>(A)->clear(); },
        +[](void* D, const void* S) { *static_cast<TArray<T>*>(D) = *static_cast<const TArray<T>*>(S); },
    };
    return &s;
}
```

따라서 `FArrayProperty` 는 두 가지를 들고 다닌다:

```cpp
class FArrayProperty final : public FProperty {
public:
    std::unique_ptr<FProperty> Inner;    // 요소 디스크립터 (재귀)
    FArrayAccessor*            Accessor; // T 의존 함수 포인터 테이블
};
```

`Inner` 의 `Offset_Internal` 은 **0** 이다. `Accessor->GetAt(Array, i)` 가 반환하는 포인터 자체가 요소의 시작점이므로, inner 디스크립터에는 추가 오프셋이 필요 없다.

### 3-3. Codegen 산출물
[`StaticMeshComponent.gen.cpp`](../KraftonEngine/Intermediate/Generated/Source/StaticMeshComponent.gen.cpp) 에서 발췌:

```cpp
Cls->AddProperty(new FArrayProperty(
    "Materials", "Materials", CPF_Edit | CPF_FixedSize,
    offsetof(ThisClass, MaterialSlots),
    sizeof(((ThisClass*)0)->MaterialSlots),
    std::unique_ptr<FProperty>(new FMaterialSlotProperty(
        "Element", "Materials", CPF_None, 0, sizeof(FMaterialSlot))),
    GetTArrayAccessor<FMaterialSlot>()));
```

원소 타입이 enum/struct 면 inner 디스크립터도 그에 맞춰 만들어진다 ([`GenerateCode.py:866-907`](../Scripts/GenerateCode.py#L866-L907)).

### 3-4. 직렬화
JSON 배열로 직렬화한다.

```cpp
// FixedSize 플래그가 켜져 있으면 (ex. 머티리얼 슬롯) Clear 하지 않고 in-place 로
// 덮어쓰며, 부족하면 잘라낸다. 이는 호스트(예: StaticMeshComponent)가 슬롯 개수를
// 자체 로직으로 미리 잡아두기 때문이다.
```

### 3-5. 에디터 UI
[`EditorPropertyWidget.cpp:1589`](../KraftonEngine/Source/Editor/UI/EditorPropertyWidget.cpp#L1589): 헤더("N elements"), 추가/삭제 버튼(고정 사이즈가 아닐 때), 각 요소를 `RenderPropertyWidget` 에 재귀적으로 넘긴다. 요소 이름은 `"Element <i>"` 컨벤션 — 머티리얼 슬롯이 인덱스 추출에 의존하므로 절대 바꾸면 안 되는 약속이다.

### 3-6. 한계
- **`TArray<UObject*>`** 는 inner 가 `FObjectProperty` 여야 동작하지만, 그것이 **빈 스텁**이라 현재 못 쓴다 (§6 참고).
- **`TArray<USTRUCT>`** 는 v1 차단([`GenerateCode.py:660`](../Scripts/GenerateCode.py#L660)).
- 정렬·키 기반 `TMap` 류는 미지원.

---

## 4. `FSceneComponentRefProperty` — 컴포넌트 경로 참조 (특수 케이스)

이건 "포인터 같지만 사실은 문자열 경로" 인 **레거시 인터럽**이다.

### 4-1. 동기
`UMovementComponent::UpdatedComponent` 같은 멤버는 *"같은 액터 안의 어떤 SceneComponent" 를 가리켜야* 한다. raw 포인터를 들고 다니면 직렬화가 어렵고 (포인터를 다시 디스크 표현으로 매핑해야 함), 컴포넌트 이름이 바뀌면 즉시 깨진다.

### 4-2. 저장 표현
멤버 타입이 `FString` 이고, 내용물은 컴포넌트 경로 문자열(예: `"Root/Body"`). codegen 의 [`POINTER_TYPE_MAP`](../Scripts/GenerateCode.py#L281-L283) 에 `USceneComponent*` → `EPropertyType::SceneComponentRef` 매핑이 등록되어 있다.

### 4-3. 디스크립터
[`FSceneComponentRefProperty`](../KraftonEngine/Source/Engine/Core/Property/PropertyTypes.h) 는 추가 메타데이터 없이 `FString` 그대로 직렬화한다.

### 4-4. 에디터 UI
[`EditorPropertyWidget.cpp:1212`](../KraftonEngine/Source/Editor/UI/EditorPropertyWidget.cpp#L1212): 호스트가 `UMovementComponent` 임을 검사하고, 자기 액터의 SceneComponent 목록을 콤보 박스로 그린 뒤 선택된 항목의 경로 문자열을 저장한다.

### 4-5. 한계 / 향후
- **현재 UI 는 `UMovementComponent` 전용**으로 하드코딩되어 있다.
- 이상적으로는 **`FObjectProperty`** 가 도입되면 이 타입을 흡수해야 한다 (`FObjectProperty(PropertyClass=USceneComponent)` + Outer-relative path 표현). 그러나 컴포넌트는 자산이 아니므로 `FSoftObjectProperty` 로 흡수되지는 않는다.

---

## 5. `FSoftObjectProperty` — 자산에 대한 약/지연 참조

### 5-1. 목적
> *"액터가 자산을 '안다'고 해서, 그 자산을 메모리에 올려두고 싶지는 않다."*

`UPROPERTY(Class=UStaticMesh)` 같은 어노테이션과 함께 사용하면, 멤버는 자산 경로만 들고 다니다가 필요한 순간 `Get()` 호출에서 로드된다.

### 5-2. 저장 표현
멤버 타입은 [`TSoftObjectPtr<T>`](../KraftonEngine/Source/Engine/Core/UObject/TSoftObjectPtr.h), 베이스는 [`FSoftObjectPtr`](../KraftonEngine/Source/Engine/Core/UObject/FSoftObjectPtr.h):

```cpp
class FSoftObjectPtr {
protected:
    FSoftObjectPath          Path;            // 문자열 경로
    mutable FWeakObjectPtr   CachedObject;    // 로드 후 캐시 (Index+Serial)
};
```

`TSoftObjectPtr<T>` 는 데이터 멤버가 없는 typed wrapper다(`static_assert(sizeof(...) == sizeof(FSoftObjectPtr))`).

### 5-3. 디스크립터 — `FSoftObjectProperty`
[`FSoftObjectProperty`](../KraftonEngine/Source/Engine/Core/Property/FObjectPropertyBase/FSoftObjectProperty.h)는 `FObjectPropertyBase` 를 상속하여 다음 가상 메서드 두 개를 구현한다.

```cpp
class FObjectPropertyBase : public FProperty {
public:
    virtual UObject* GetObjectPropertyValue(void* PropAddr) const = 0;
    virtual void     SetObjectPropertyValue(void* PropAddr, UObject* Value) const = 0;

    UClass* PropertyClass = nullptr;  // 허용되는 타입(예: UStaticMesh::StaticClass())
};
```

소프트 구현:

```cpp
// FSoftObjectProperty.cpp
UObject* FSoftObjectProperty::GetObjectPropertyValue(void* Addr) const {
    return static_cast<FSoftObjectPtr*>(Addr)->Get();
    // 미로드 상태면 ResolveObject() 가 GUObjectArray 순회 → 자산이 이미 메모리에
    // 있으면 캐시에 채우고 반환, 없으면 nullptr.
}

void FSoftObjectProperty::SetObjectPropertyValue(void* Addr, UObject* Value) const {
    auto* Ptr = static_cast<FSoftObjectPtr*>(Addr);
    if (!Value || (PropertyClass && !Value->IsA(PropertyClass))) {
        Ptr->Reset();
        return;
    }
    Ptr->SetPath(Value->GetAssetPathFileName());
    Ptr->SetCache(Value);
}
```

타입 안전성 체크는 디스크립터의 `PropertyClass` 에서 한다. *"Data is dumb. Metadata is smart."*

### 5-4. Codegen 어노테이션
```cpp
UPROPERTY(Edit, Category="Mesh", Class=UStaticMesh)
TSoftObjectPtr<UStaticMesh> StaticMesh;
```

[`GenerateCode.py:826-831`](../Scripts/GenerateCode.py#L826-L831) 에서 `Class=` 를 받아 `PropertyClass` 인자로 emit:

```cpp
new FSoftObjectProperty(
    "Static Mesh", "Mesh", CPF_Edit,
    offsetof(ThisClass, StaticMesh),
    sizeof(((ThisClass*)0)->StaticMesh),
    UStaticMesh::StaticClass());
```

### 5-5. 직렬화
경로 문자열만 JSON 으로 나간다. 캐시는 직렬화 대상 아님.

```cpp
// 저장: "/Game/Mesh/Tank.uasset"
// 로딩: SetPath만 호출 → Get() 첫 호출 때 ResolveObject 시도
```

[`FSoftObjectPath::ResolveObject`](../KraftonEngine/Source/Engine/Core/UObject/FSoftObjectPath.cpp) 는 `GUObjectArray` 를 선형 순회하면서 `GetAssetPathFileName()` 이 일치하는 객체를 찾는다 — 따라서 객체가 이미 로드되어 있어야 hit한다. 자동 로딩 트리거는 아직 없다.

### 5-6. 에디터 UI
[`EditorPropertyWidget.cpp:1268`](../KraftonEngine/Source/Editor/UI/EditorPropertyWidget.cpp#L1268): `PropertyClass` 가 무엇이냐에 따라 분기하여 ImGui 콤보 박스 + Import 버튼을 그린다. `UStaticMesh`, `USkeletalMesh` 등 자산 클래스별로 별도 분기가 있다.

### 5-7. 한계
- `ResolveObject` 가 O(N) 선형 검색 → 자산 수가 늘면 비용 증가. 경로 → 객체 해시 테이블이 필요.
- 미로드 자산을 디스크에서 자동으로 끌어오는 로더 훅 없음 — 누군가가 미리 로드해야 한다.
- 에디터 UI의 `PropertyClass` 분기가 하드코딩 — `if/else` 사슬이 늘어남.

---

## 6. `FObjectProperty` / `FClassProperty` — 하드 참조 *(빈 스텁; 의도 설계)*

현재 두 헤더 모두 `#pragma once` 한 줄뿐이다.

```cpp
// FObjectProperty.h
#pragma once

// FClassProperty.h
#pragma once
```

본 절은 **이 두 클래스가 채워지면 어떻게 동작해야 하는지** 를 설계 수준에서 명세한다. GC 도입의 사전 조건이기도 하다 ([`GCImplementationStatus.md`](GCImplementationStatus.md) §4-1).

### 6-1. 의도된 저장 표현
저자 코드:

```cpp
UPROPERTY(Edit, Category="Aim")
AActor* TargetActor = nullptr;

UPROPERTY(Edit, Category="Spawn")
UClass* SpawnableClass = nullptr;
```

멤버 타입은 그냥 `T*` (또는 향후 `TObjectPtr<T>` 래퍼). 메모리에 raw 포인터가 살아 있다.

### 6-2. `FObjectProperty` 의도 명세

```cpp
class FObjectProperty : public FObjectPropertyBase {
public:
    FObjectProperty(const FString& InName, const FString& InCategory,
                    uint32 InFlag, uint32 InOffset, uint32 InSize,
                    UClass* InPropertyClass)
        : FObjectPropertyBase(InName, InCategory, InFlag, InOffset, InSize, InPropertyClass)
    {}

    EPropertyType GetType() const override { return EPropertyType::Object; } // 신규 enum 값
    json::JSON Serialize(const void* Instance) const override;
    void       Deserialize(void* Instance, const json::JSON& Value) const override;

    UObject* GetObjectPropertyValue(void* Addr) const override {
        return *static_cast<UObject* const*>(Addr);   // 그냥 포인터 deref
    }

    void SetObjectPropertyValue(void* Addr, UObject* Value) const override {
        if (Value && PropertyClass && !Value->IsA(PropertyClass)) {
            return;  // 타입 미스매치는 무시 (또는 로그)
        }
        *static_cast<UObject**>(Addr) = Value;
    }
};
```

**핵심 동작 — 직렬화**:

```cpp
json::JSON FObjectProperty::Serialize(const void* Instance) const {
    UObject* Obj = *static_cast<UObject* const*>(ContainerPtrToValuePtr(Instance));
    if (!Obj) return json::JSON("");

    // 결정 사항: 어떤 식별자로 저장할 것인가?
    //   A) UUID 기반 (동일 씬 내부에서만 valid; 액터 간 약참조에 적합)
    //   B) 자산 경로 기반 (자산이라면 FSoftObjectProperty와 동일)
    //   C) Outer-relative 이름 경로 (Unreal 의 GetPathName() 방식)
    // 현재 엔진은 A) UUID 가 가장 자연스럽다 — UObject 마다 UUID 가 이미 있고
    // GUObjectArray::FindByUUID 가 이미 존재한다.
    return json::JSON(std::to_string(Obj->GetUUID()));
}

void FObjectProperty::Deserialize(void* Instance, const json::JSON& Value) const {
    UObject** Target = static_cast<UObject**>(ContainerPtrToValuePtr(Instance));
    const FString S = Value.ToString();
    if (S.empty()) { *Target = nullptr; return; }

    UObject* Resolved = GUObjectArray.FindByUUID(static_cast<uint32>(std::stoul(S)));
    // PropertyClass 가 강제되어 있다면 IsA 체크
    if (Resolved && PropertyClass && !Resolved->IsA(PropertyClass)) {
        Resolved = nullptr;
    }
    *Target = Resolved;
}
```

> ⚠️ UUID 기반은 **씬 로딩 패스 순서** 문제를 만든다 — 참조되는 객체가 참조하는 객체보다 먼저 로드되어야 한다. 현재 [`FSceneSaveManager`](../KraftonEngine/Source/Engine/Serialization/SceneSaveManager.cpp) 는 이미 2-패스 로드(객체 생성 → 프로퍼티 채우기)를 하므로 자연스럽게 들어맞는다.

**핵심 동작 — GC 통합**:

GC 의 mark 페이즈에서 객체 그래프를 순회할 때 `FObjectProperty` 는 다음 역할을 한다.

```cpp
// 가상의 AddReferencedObjects 기본 구현
void UObject::AddReferencedObjects(FReferenceCollector& Collector) {
    TArray<const FProperty*> Props;
    GetAllProperties(Props);
    for (const FProperty* P : Props) {
        if (auto* OP = dynamic_cast<const FObjectPropertyBase*>(P)) {
            void* Addr = OP->ContainerPtrToValuePtr(this);
            UObject* Ref = OP->GetObjectPropertyValue(Addr);
            if (Ref) Collector.AddReferencedObject(Ref);
            // 주의: FSoftObjectProperty 의 GetObjectPropertyValue 는 미로드 시 nullptr
            // 을 반환하므로 자연스럽게 강참조에서 빠진다 — 이게 "소프트" 의 정의.
        }
        // FArrayProperty/FStructProperty 의 inner 도 재귀 순회
    }
}
```

즉 `FObjectProperty` 가 GC 에 노출하는 것은 **하드 참조**, `FSoftObjectProperty` 는 **약 참조**. 같은 가상 인터페이스 `GetObjectPropertyValue` 를 통해 두 의미가 다르게 표현된다.

### 6-3. `FClassProperty` 의도 명세

`FClassProperty` 는 `FObjectProperty` 와 동일한 메모리 표현(`UObject*` 슬롯)을 갖지만, **항상 `UClass` 인스턴스**를 가리킨다. 두 가지 차이:

1. **`PropertyClass`** 는 "이 슬롯이 가리킬 수 있는 UClass 의 베이스 제약" 을 의미한다 (예: `MetaClass = AActor::StaticClass()` → "AActor 의 자식 클래스만").
2. **직렬화**는 UUID 가 아니라 클래스 이름 문자열이 자연스럽다 (UClass 는 정적 객체이고 이름이 유일하므로).

```cpp
class FClassProperty : public FObjectProperty {
public:
    UClass* MetaClass = nullptr;  // PropertyClass 와 별개의 추가 제약

    json::JSON Serialize(const void* Instance) const override {
        UClass* C = static_cast<UClass*>(*static_cast<UObject* const*>(ContainerPtrToValuePtr(Instance)));
        return json::JSON(C ? FString(C->GetName()) : FString(""));
    }
    void Deserialize(void* Instance, const json::JSON& Value) const override {
        UClass* Found = UClass::FindByName(Value.ToString().c_str());
        if (Found && MetaClass && !Found->IsChildOf(MetaClass)) Found = nullptr;
        *static_cast<UObject**>(ContainerPtrToValuePtr(Instance)) = Found;
    }
};
```

### 6-4. Codegen 통합
[`GenerateCode.py`](../Scripts/GenerateCode.py) 측에서 필요한 변경:

1. `POINTER_TYPE_MAP` 에 `"UObject"` (그리고 모든 UClass-derived) → `EPropertyType::Object` 매핑 추가. 현재는 `USceneComponent` 만 매핑되어 있고 그것도 `SceneComponentRef` 라는 특수 케이스다.
2. 더 일반적으로는, **포인터형 멤버 + 알려진 `UCLASS` 타입의 조합**을 만나면 자동으로 `FObjectProperty(PropertyClass=<TheClass>::StaticClass())` 로 emit.
3. `UClass*` 멤버는 `FClassProperty` 로 분기. 어노테이션으로 `MetaClass=AActor` 같은 추가 제약 지원.
4. `PROPERTY_HEADER_BY_TYPE` 에 `EPropertyType::Object`, `EPropertyType::Class` → `Core/Property/FObjectPropertyBase/FObjectProperty.h`, `FClassProperty.h` 추가.

### 6-5. 에디터 UI 의도
- **`FObjectProperty`**: 콤보 박스에 `GUObjectArray` 의 `PropertyClass` 자식들을 나열. 드래그&드롭으로 씬 트리에서 객체를 떨어뜨릴 수 있게 함.
- **`FClassProperty`**: 콤보 박스에 `UClass::GetAllClasses()` 중 `MetaClass` 자식만 필터링하여 나열.

### 6-6. 도입 시 결정해야 할 것
- **포인터 vs `TObjectPtr<T>` 래퍼**: UE5처럼 raw 포인터를 래퍼로 감쌀 것인가? 현재 코드베이스는 raw `T*` 가 압도적이므로 래퍼 도입은 점진적 마이그레이션 비용을 동반한다.
- **PIE/씬 간 참조 정책**: 한 씬의 액터가 다른 씬의 액터를 참조하면 어떻게 할 것인가? (현재는 단일 World 전제라 큰 이슈 없음.)
- **댕글링 정책**: GC 도입 전까지는 `FObjectProperty` 가 참조하는 객체가 다른 곳에서 `DestroyObject` 되어 사라질 수 있다. 직렬화 시 `IsValid` 체크가 안전망. 장기적으로는 GC 가 raw 포인터를 자동으로 null 처리하거나, `TObjectPtr` 가 stale 감지를 한다.

### 6-7. 한 줄 결론
`FObjectProperty` / `FClassProperty` 는 *기술적으로* 단순(포인터 deref + 타입 체크)하지만, *정책적으로*는 직렬화 표현과 GC 의미를 동시에 결정해야 하므로 도입 비용이 코드보다 설계에 있다.

---

## 7. 비교 요약표

| 디스크립터 | 추가 메타데이터 | 직렬화 표현 | GC 의미 (예정) |
|---|---|---|---|
| `FEnumProperty` | `UEnum*` (메타 객체) | enum 본래 값 (raw int, int32 다운캐스트) | — (값 타입) |
| `FStructProperty` | `UScriptStruct*` (메타 객체 + `ICppStructOps`) | JSON 객체 (자식 재귀) | 자식 재귀 |
| `FArrayProperty` | `Inner` 디스크립터, `Accessor` 함수 테이블 | JSON 배열 (요소 재귀) | 요소 재귀 |
| `FSceneComponentRefProperty` | 없음 (FString 그대로) | 경로 문자열 | — (문자열) |
| `FSoftObjectProperty` | `PropertyClass` | 경로 문자열 | **약 참조** (미로드 시 nullptr) |
| `FObjectProperty` *(예정)* | `PropertyClass` | UUID 문자열 | **강 참조** |
| `FClassProperty` *(예정)* | `PropertyClass`, `MetaClass` | 클래스 이름 문자열 | **강 참조** (UClass 는 항상 정적) |

---

## 8. 한 줄 요약

> **enum 은 `UEnum` 메타 객체, struct 는 `UScriptStruct` 메타 객체(+`ICppStructOps`), 배열은 타입 소거 액세서, soft 참조는 경로+캐시, hard 참조는 (도입 시) raw 포인터+`PropertyClass` 제약** — 자유 함수 포인터·raw 배열로 흩어져 있던 메타데이터가 **`UObject → UField` 계층 안의 정식 메타 객체로 통합**되면서, 리플렉션·직렬화·GC·향후 상속이 모두 같은 인터페이스(`UStruct::GetProperties`, `IsA<UEnum>`, `Cast<UScriptStruct>`) 위에서 동작하게 되었다. 남은 큰 작업은 여전히 [`FObjectProperty`](../KraftonEngine/Source/Engine/Core/Property/FObjectPropertyBase/FObjectProperty.h) / [`FClassProperty`](../KraftonEngine/Source/Engine/Core/Property/FObjectPropertyBase/FClassProperty.h) 의 빈 스텁을 채우는 정책 결정이다.
