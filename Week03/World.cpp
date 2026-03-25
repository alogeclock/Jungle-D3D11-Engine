#include "World.h"
#include "Source/Engine/Object/Public/Actor.h"

#include "Source/Editor/Public/Grid.h"
#include "Source/Editor/Public/PivotTransformGizmo.h"
#include "Source/Editor/Public/Axis.h"
#include "Source/Editor/Public/EditorSpriteActor.h"

// 모든 Primitive Component 헤더 포함
#include "Source/Engine/Public/Classes/Components/SphereComponent.h"
#include "Source/Engine/Public/Classes/Components/CubeComponent.h"
#include "Source/Engine/Public/Classes/Components/TriangleComponent.h"
#include "Source/Engine/Public/Classes/Components/PlaneComponent.h"
#include "Source/Engine/Public/Classes/Components/TextComponent.h"
#include "Source/Engine/Public/Classes/Components/UUIDTextComponent.h"
#include "Source/Engine/Public/Classes/Components/ParticleSubUVComponent.h"
#include "Source/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Source/Engine/Public/Classes/Components/CubeArrowComponent.h"
#include "Source/Engine/Public/Classes/Components/RingComponent.h"
#include "Source/Engine/Public/Classes/Components/AxisComponent.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"
#include "Source/Engine/Public/GUI/ImGuiManager.h"
#include "Source/Editor/Public/EditorEngine.h"
#include "EngineStatics.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

struct FHitResult;

namespace
{
constexpr uint32 SceneFormatVersion = 2;

struct FSceneLoadContext
{
    uint32 SceneVersion = 0;
    TMap<uint32, UObject*> ObjectsBySavedId;
    TArray<FString> Errors;

    void AddError(const FString& Message)
    {
        Errors.push_back(Message);
    }
};

std::string WStringToUTF8(const std::wstring& Source)
{
    if (Source.empty())
    {
        return {};
    }

    int SizeNeeded = WideCharToMultiByte(CP_UTF8, 0, Source.c_str(), static_cast<int>(Source.size()), nullptr, 0, nullptr, nullptr);
    std::string Result(SizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, Source.c_str(), static_cast<int>(Source.size()), Result.data(), SizeNeeded, nullptr, nullptr);
    return Result;
}

std::string GetCurrentTimestampUtc()
{
    const auto Now = std::chrono::system_clock::now();
    const std::time_t Time = std::chrono::system_clock::to_time_t(Now);
    std::tm UtcTime {};
    gmtime_s(&UtcTime, &Time);

    std::ostringstream Stream;
    Stream << std::put_time(&UtcTime, "%Y-%m-%dT%H:%M:%SZ");
    return Stream.str();
}

json ToJson(const FVector<float>& Value)
{
    return json::array({Value.X, Value.Y, Value.Z});
}

json ToJson(const FVector4<float>& Value)
{
    return json::array({Value.X, Value.Y, Value.Z, Value.W});
}

json ToJson(const FTransform& Value)
{
    json Result;
    Result["location"] = ToJson(Value.Location);
    Result["rotation"] = ToJson(Value.Rotation);
    Result["scale"] = ToJson(Value.Scale);
    return Result;
}

bool TryReadVector3(const json& Value, FVector<float>& OutVector)
{
    if (!Value.is_array() || Value.size() != 3)
    {
        return false;
    }

    OutVector.X = Value[0].get<float>();
    OutVector.Y = Value[1].get<float>();
    OutVector.Z = Value[2].get<float>();
    return true;
}

bool TryReadVector4(const json& Value, FVector4<float>& OutVector)
{
    if (!Value.is_array() || Value.size() != 4)
    {
        return false;
    }

    OutVector.X = Value[0].get<float>();
    OutVector.Y = Value[1].get<float>();
    OutVector.Z = Value[2].get<float>();
    OutVector.W = Value[3].get<float>();
    return true;
}

bool TryReadTransform(const json& Value, FTransform& OutTransform)
{
    if (!Value.is_object())
    {
        return false;
    }

    return Value.contains("location") && Value.contains("rotation") && Value.contains("scale") &&
           TryReadVector3(Value["location"], OutTransform.Location) && TryReadVector3(Value["rotation"], OutTransform.Rotation) &&
           TryReadVector3(Value["scale"], OutTransform.Scale);
}

bool ShouldSkipReflectedProperty(const FProperty& Property)
{
    return Property.Name == "OwnedComponents" || Property.Name == "RootComponent";
}

json SerializePropertyValue(const UObject* Object, const FProperty& Property)
{
    const void* ValuePtr = Property.GetValuePtr(Object);

    switch (Property.Type)
    {
    case EPropertyType::Int:
        return *reinterpret_cast<const int32*>(ValuePtr);
    case EPropertyType::Float:
        return *reinterpret_cast<const float*>(ValuePtr);
    case EPropertyType::Bool:
        return *reinterpret_cast<const bool*>(ValuePtr);
    case EPropertyType::String:
        return *reinterpret_cast<const FString*>(ValuePtr);
    case EPropertyType::Vec3:
        return ToJson(*reinterpret_cast<const FVector<float>*>(ValuePtr));
    case EPropertyType::Transform:
        return ToJson(*reinterpret_cast<const FTransform*>(ValuePtr));
    case EPropertyType::UObjectPtr:
    {
        UObject* const* ReferencedObject = reinterpret_cast<UObject* const*>(ValuePtr);
        return (*ReferencedObject != nullptr) ? json((*ReferencedObject)->GetUUID()) : json(nullptr);
    }
    default:
        return json();
    }
}

bool DeserializePropertyValue(UObject* Object, const FProperty& Property, const json& Value, FSceneLoadContext& LoadContext)
{
    void* ValuePtr = Property.GetValuePtr(Object);

    try
    {
        switch (Property.Type)
        {
        case EPropertyType::Int:
            *reinterpret_cast<int32*>(ValuePtr) = Value.get<int32>();
            return true;
        case EPropertyType::Float:
            *reinterpret_cast<float*>(ValuePtr) = Value.get<float>();
            return true;
        case EPropertyType::Bool:
            *reinterpret_cast<bool*>(ValuePtr) = Value.get<bool>();
            return true;
        case EPropertyType::String:
            *reinterpret_cast<FString*>(ValuePtr) = Value.get<FString>();
            return true;
        case EPropertyType::Vec3:
            return TryReadVector3(Value, *reinterpret_cast<FVector<float>*>(ValuePtr));
        case EPropertyType::Transform:
            return TryReadTransform(Value, *reinterpret_cast<FTransform*>(ValuePtr));
        case EPropertyType::UObjectPtr:
            return true;
        default:
            return false;
        }
    }
    catch (const std::exception& Exception)
    {
        LoadContext.AddError("[DeserializeProperty] " + Object->GetName().ToString() + "." + Property.Name + ": " + Exception.what());
        return false;
    }
}

void ResolvePropertyReference(UObject* Object, const FProperty& Property, const json& Value, FSceneLoadContext& LoadContext)
{
    if (Property.Type != EPropertyType::UObjectPtr || Value.is_null())
    {
        return;
    }

    UObject** ReferencedObject = reinterpret_cast<UObject**>(Property.GetValuePtr(Object));
    const uint32 SavedId = Value.get<uint32>();
    auto It = LoadContext.ObjectsBySavedId.find(SavedId);
    if (It == LoadContext.ObjectsBySavedId.end())
    {
        LoadContext.AddError("[ResolveReference] Missing object id " + std::to_string(SavedId) + " for " + Object->GetName().ToString() + "." + Property.Name);
        return;
    }

    *ReferencedObject = It->second;
}

json SerializeReflectedProperties(const UObject* Object)
{
    json PropertiesJson = json::object();
    UClass* ObjectClass = Object->GetClass();
    if (ObjectClass == nullptr)
    {
        return PropertiesJson;
    }

    for (const FProperty& Property : ObjectClass->GetProperties())
    {
        if (ShouldSkipReflectedProperty(Property))
        {
            continue;
        }

        json Value = SerializePropertyValue(Object, Property);
        if (!Value.is_discarded())
        {
            PropertiesJson[Property.Name] = Value;
        }
    }

    return PropertiesJson;
}

void DeserializeReflectedProperties(UObject* Object, const json& PropertiesJson, FSceneLoadContext& LoadContext)
{
    if (!PropertiesJson.is_object())
    {
        return;
    }

    UClass* ObjectClass = Object->GetClass();
    if (ObjectClass == nullptr)
    {
        return;
    }

    for (const FProperty& Property : ObjectClass->GetProperties())
    {
        if (ShouldSkipReflectedProperty(Property))
        {
            continue;
        }

        auto It = PropertiesJson.find(Property.Name);
        if (It == PropertiesJson.end())
        {
            continue;
        }

        DeserializePropertyValue(Object, Property, *It, LoadContext);
    }
}

void ResolveReflectedProperties(UObject* Object, const json& PropertiesJson, FSceneLoadContext& LoadContext)
{
    if (!PropertiesJson.is_object())
    {
        return;
    }

    UClass* ObjectClass = Object->GetClass();
    if (ObjectClass == nullptr)
    {
        return;
    }

    for (const FProperty& Property : ObjectClass->GetProperties())
    {
        if (ShouldSkipReflectedProperty(Property))
        {
            continue;
        }

        auto It = PropertiesJson.find(Property.Name);
        if (It == PropertiesJson.end())
        {
            continue;
        }

        ResolvePropertyReference(Object, Property, *It, LoadContext);
    }
}

UClass* FindClassByName(const FString& ClassName)
{
    static const TMap<FString, UClass*> ClassMap = {
        {"AActor", AActor::StaticClass()},
        {"USceneComponent", USceneComponent::StaticClass()},
        {"UPrimitiveComponent", UPrimitiveComponent::StaticClass()},
        {"USphereComponent", USphereComponent::StaticClass()},
        {"UCubeComponent", UCubeComponent::StaticClass()},
        {"UTriangleComponent", UTriangleComponent::StaticClass()},
        {"UPlaneComponent", UPlaneComponent::StaticClass()},
        {"UTextComponent", UTextComponent::StaticClass()},
        {"UUUIDTextComponent", UUUIDTextComponent::StaticClass()},
        {"UParticleSubUVComponent", UParticleSubUVComponent::StaticClass()},
        {"UArrowComponent", UArrowComponent::StaticClass()},
        {"UCubeArrowComponent", UCubeArrowComponent::StaticClass()},
        {"URingComponent", URingComponent::StaticClass()},
        {"UAxisComponent", UAxisComponent::StaticClass()}
    };

    auto It = ClassMap.find(ClassName);
    return (It != ClassMap.end()) ? It->second : nullptr;
}

json SerializeComponent(UActorComponent* Component)
{
    json ComponentJson;
    ComponentJson["id"] = Component->GetUUID();
    ComponentJson["class"] = Component->GetClass()->GetName();
    ComponentJson["name"] = Component->GetName().ToString();
    ComponentJson["active"] = Component->IsActive();
    ComponentJson["properties"] = SerializeReflectedProperties(Component);

    if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
    {
        ComponentJson["transform"] = ToJson(SceneComponent->GetTransform());
        ComponentJson["color"] = ToJson(SceneComponent->GetColor());
        ComponentJson["attach_parent_id"] =
            (SceneComponent->GetAttachParent() != nullptr) ? json(SceneComponent->GetAttachParent()->GetUUID()) : json(nullptr);
    }

    if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
    {
        json RuntimeState;
        RuntimeState["primitive_type"] = static_cast<uint32>(PrimitiveComponent->GetPrimitiveType());
        RuntimeState["topology"] = static_cast<uint32>(PrimitiveComponent->GetTopology());
        RuntimeState["cull_mode"] = static_cast<uint32>(PrimitiveComponent->GetCullMode());
        RuntimeState["depth_test"] = PrimitiveComponent->GetEnableDepthTest();
        RuntimeState["visible"] = PrimitiveComponent->IsVisible();
        RuntimeState["editor_only"] = PrimitiveComponent->IsInEditor();
        ComponentJson["runtime_state"] = RuntimeState;
    }

    if (UTextComponent* TextComponent = Cast<UTextComponent>(Component))
    {
        json Resources;
        Resources["text_texture"] = TextComponent->GetFilePath();
        Resources["text_texture_kor"] = TextComponent->GetKoreanFilePath();
        ComponentJson["resources"] = Resources;
        ComponentJson["custom"] = {{"text", TextComponent->GetText()}};
    }
    else if (UParticleSubUVComponent* ParticleComponent = Cast<UParticleSubUVComponent>(Component))
    {
        json Resources;
        Resources["texture"] = ParticleComponent->FilePath;
        ComponentJson["resources"] = Resources;
        ComponentJson["custom"] = {
            {"width", ParticleComponent->Width},
            {"height", ParticleComponent->Height},
            {"sprite_size", ParticleComponent->SpriteSize},
            {"total_frame_count", ParticleComponent->TotalFrameCount}
        };
    }

    return ComponentJson;
}

json SerializeActor(AActor* Actor)
{
    json ActorJson;
    ActorJson["id"] = Actor->GetUUID();
    ActorJson["class"] = Actor->GetClass()->GetName();
    ActorJson["name"] = Actor->GetName().ToString();
    ActorJson["active"] = Actor->IsActive();
    ActorJson["properties"] = SerializeReflectedProperties(Actor);
    ActorJson["transform"] = ToJson(Actor->GetTransform());
    ActorJson["root_component_id"] = (Actor->GetRootComponent() != nullptr) ? json(Actor->GetRootComponent()->GetUUID()) : json(nullptr);

    json Components = json::array();
    for (UActorComponent* Component : Actor->GetOwnedComponents())
    {
        if (Component != nullptr)
        {
            Components.push_back(SerializeComponent(Component));
        }
    }
    ActorJson["components"] = Components;
    return ActorJson;
}

void ApplyComponentJson(UActorComponent* Component, const json& ComponentJson, FSceneLoadContext& LoadContext)
{
    if (ComponentJson.contains("name") && ComponentJson["name"].is_string())
    {
        Component->SetName(ComponentJson["name"].get<FString>());
    }
    if (ComponentJson.contains("active"))
    {
        Component->SetActive(ComponentJson["active"].get<bool>());
    }
    if (ComponentJson.contains("properties"))
    {
        DeserializeReflectedProperties(Component, ComponentJson["properties"], LoadContext);
    }

    if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
    {
        if (ComponentJson.contains("transform"))
        {
            FTransform Transform;
            if (TryReadTransform(ComponentJson["transform"], Transform))
            {
                SceneComponent->SetTransform(Transform);
            }
        }

        if (ComponentJson.contains("color"))
        {
            FVector4<float> Color;
            if (TryReadVector4(ComponentJson["color"], Color))
            {
                SceneComponent->SetColor(Color);
            }
        }
    }

    if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
    {
        const json RuntimeState = ComponentJson.value("runtime_state", json::object());
        if (RuntimeState.contains("primitive_type"))
        {
            PrimitiveComponent->SetPrimitiveType(static_cast<EPrimitiveType>(RuntimeState["primitive_type"].get<uint32>()));
        }
        if (RuntimeState.contains("topology"))
        {
            PrimitiveComponent->SetTopology(static_cast<D3D11_PRIMITIVE_TOPOLOGY>(RuntimeState["topology"].get<uint32>()));
        }
        if (RuntimeState.contains("cull_mode"))
        {
            PrimitiveComponent->SetCullMode(static_cast<ECullMode>(RuntimeState["cull_mode"].get<uint32>()));
        }
        if (RuntimeState.contains("depth_test"))
        {
            PrimitiveComponent->SetEnableDepthTest(RuntimeState["depth_test"].get<bool>());
        }
        if (RuntimeState.contains("visible"))
        {
            PrimitiveComponent->SetVisible(RuntimeState["visible"].get<bool>());
        }
        if (RuntimeState.contains("editor_only"))
        {
            PrimitiveComponent->SetIsInEditor(RuntimeState["editor_only"].get<bool>());
        }
    }

    if (UTextComponent* TextComponent = Cast<UTextComponent>(Component))
    {
        const json Resources = ComponentJson.value("resources", json::object());
        const json Custom = ComponentJson.value("custom", json::object());

        if (Resources.contains("text_texture"))
        {
            TextComponent->SetFilePath(Resources["text_texture"].get<FString>());
        }
        if (Resources.contains("text_texture_kor"))
        {
            TextComponent->SetKoreanFilePath(Resources["text_texture_kor"].get<FString>());
        }
        if (Custom.contains("text"))
        {
            TextComponent->SetText(Custom["text"].get<FString>());
        }
    }
    else if (UParticleSubUVComponent* ParticleComponent = Cast<UParticleSubUVComponent>(Component))
    {
        const json Resources = ComponentJson.value("resources", json::object());
        const json Custom = ComponentJson.value("custom", json::object());

        if (Resources.contains("texture"))
        {
            ParticleComponent->FilePath = Resources["texture"].get<FString>();
        }
        if (Custom.contains("width"))
        {
            ParticleComponent->Width = Custom["width"].get<uint32>();
        }
        if (Custom.contains("height"))
        {
            ParticleComponent->Height = Custom["height"].get<uint32>();
        }
        if (Custom.contains("sprite_size"))
        {
            ParticleComponent->SpriteSize = Custom["sprite_size"].get<uint32>();
        }
        if (Custom.contains("total_frame_count"))
        {
            ParticleComponent->TotalFrameCount = Custom["total_frame_count"].get<uint32>();
        }
    }
}

void ResolveComponentJson(UActorComponent* Component, const json& ComponentJson, FSceneLoadContext& LoadContext)
{
    if (ComponentJson.contains("properties"))
    {
        ResolveReflectedProperties(Component, ComponentJson["properties"], LoadContext);
    }

    USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
    if (SceneComponent == nullptr)
    {
        return;
    }

    const json AttachParentId = ComponentJson.value("attach_parent_id", json(nullptr));
    if (AttachParentId.is_null())
    {
        return;
    }

    const uint32 SavedParentId = AttachParentId.get<uint32>();
    auto It = LoadContext.ObjectsBySavedId.find(SavedParentId);
    if (It == LoadContext.ObjectsBySavedId.end())
    {
        LoadContext.AddError("[ResolveAttachment] Missing component id " + std::to_string(SavedParentId));
        return;
    }

    USceneComponent* AttachParent = Cast<USceneComponent>(It->second);
    if (AttachParent == nullptr)
    {
        LoadContext.AddError("[ResolveAttachment] Object id " + std::to_string(SavedParentId) + " is not a scene component");
        return;
    }

    SceneComponent->SetupAttachment(AttachParent);
}
}

UWorld::UWorld(const FString &InString) : UObject(InString) 
{
    CurrentLevel = CreateNewLevel("PersistentLevel");
    LineBatcherComponent = new ULineBatcherComponent("LineBatcherComponent");
    TextBatcherComponent = new UTextBatcherComponent("TextBatcherComponent");
}

UWorld::~UWorld()
{
    if (LineBatcherComponent)
    {
        delete LineBatcherComponent;
        LineBatcherComponent = nullptr;
    }
    if (TextBatcherComponent)
    {
        delete TextBatcherComponent;
        TextBatcherComponent = nullptr;
    }

    for (ULevel* Level : Levels)
    {
        if (Level != nullptr)
        {
            delete Level;
        }
    }
    Levels.clear();

    CurrentLevel = nullptr;
}

void UWorld::Submit(const FSceneViewOptions& ViewOptions)
{
    if (CurrentLevel)
    {
        // 현재 레벨의 모든 액터를 순회
        for (AActor *actor : CurrentLevel->GetActors())
        {
            // 액터가 소유한 모든 컴포넌트 순회
            for (UActorComponent *component : actor->GetOwnedComponents())
            {
                // PrimitiveComponent인 경우에만 Submit 호출
                UPrimitiveComponent *primitive = Cast<UPrimitiveComponent>(component);
                if (primitive != nullptr)
                {
                    primitive->Submit(ViewOptions);
                }
            }
        }
    }

    // LineBatcherComponent 등 레벨 액터 목록과 별개로 관리되는 컴포넌트가 있다면 여기서 함께 Submit 처리
    if (LineBatcherComponent)
    {
        LineBatcherComponent->Submit(ViewOptions);
    }
}

ULevel *UWorld::CreateNewLevel(const FString &NewLevelName)
{
    ULevel *NewLevel = new ULevel(NewLevelName);
    NewLevel->SetOuter(this);
    Levels.insert(NewLevel);

    return NewLevel;
}

bool UWorld::SaveLevel(const std::wstring &FilePath)
{
    if (CurrentLevel == nullptr)
        return false;

    std::filesystem::path path(FilePath);
    std::filesystem::create_directories(path.parent_path());

    json SceneJson;
    SceneJson["scene"] = {
        {"name", WStringToUTF8(path.stem().wstring())},
        {"format_version", SceneFormatVersion},
        {"saved_at_utc", GetCurrentTimestampUtc()}
    };

    json ActorsJson = json::array();
    for (AActor* Actor : CurrentLevel->GetActors())
    {
        if (Actor != nullptr)
        {
            ActorsJson.push_back(SerializeActor(Actor));
        }
    }

    SceneJson["actors"] = ActorsJson;

    std::ofstream file(path);
    if (file.is_open())
    {
        file << SceneJson.dump(2);
        file.close();
        return true;
    }

    return false;
}

bool UWorld::LoadLevel(const std::wstring &FilePath)
{
    std::filesystem::path path = std::filesystem::path(FilePath);

    std::ifstream file(path);
    if (!file.is_open())
        return false;

    json j;
     UEngineStatics::SetUUID(27);

    try
    {
        file >> j; // JSON 파싱 시도
    }
    catch (const json::parse_error&)
    {
        // JSON 형식이 잘못되었거나 파일이 비어있는 경우
        file.close();
        return false;
    }
    catch (const std::exception&)
    {
        // 기타 표준 예외 발생 시
        file.close();
        return false;
    }

    file.close();

    ULevel *OldLevel = CurrentLevel;
    FSceneLoadContext LoadContext;

    try
    {
        FString SceneName = WStringToUTF8(path.stem().wstring());
        if (j.contains("scene") && j["scene"].is_object())
        {
            const json& SceneHeader = j["scene"];
            if (SceneHeader.contains("name") && SceneHeader["name"].is_string())
            {
                SceneName = SceneHeader["name"].get<FString>();
            }
            if (SceneHeader.contains("format_version"))
            {
                LoadContext.SceneVersion = SceneHeader["format_version"].get<uint32>();
            }
        }
        else if (j.contains("SceneName") && j["SceneName"].is_string())
        {
            SceneName = j["SceneName"].get<FString>();
            LoadContext.SceneVersion = j.value("Version", 1);
        }

        CurrentLevel = CreateNewLevel(SceneName);

        const json ActorsJson = j.contains("actors") ? j["actors"] : json::array();
        struct FPendingActorLoad
        {
            AActor* Instance = nullptr;
            json Data;
        };

        struct FPendingComponentLoad
        {
            UActorComponent* Instance = nullptr;
            json Data;
        };

        TArray<FPendingActorLoad> PendingActors;
        TArray<FPendingComponentLoad> PendingComponents;

        if (ActorsJson.is_array())
        {
            for (const json& ActorJson : ActorsJson)
            {
                if (!ActorJson.contains("class") || !ActorJson.contains("id"))
                {
                    throw std::runtime_error("Actor entry missing class/id.");
                }

                UClass* ActorClass = FindClassByName(ActorJson["class"].get<FString>());
                if (ActorClass == nullptr)
                {
                    throw std::runtime_error("Unknown actor class: " + ActorJson["class"].get<FString>());
                }

                AActor* Actor = Cast<AActor>(FObjectFactory::ConstructObject(ActorClass));
                if (Actor == nullptr)
                {
                    throw std::runtime_error("Failed to create actor instance.");
                }

                Actor->SetOuter(CurrentLevel);
                CurrentLevel->GetActors().push_back(Actor);
                LoadContext.ObjectsBySavedId[ActorJson["id"].get<uint32>()] = Actor;
                PendingActors.push_back({Actor, ActorJson});

                const json ComponentsJson = ActorJson.value("components", json::array());
                if (!ComponentsJson.is_array())
                {
                    continue;
                }

                for (const json& ComponentJson : ComponentsJson)
                {
                    if (!ComponentJson.contains("class") || !ComponentJson.contains("id"))
                    {
                        throw std::runtime_error("Component entry missing class/id.");
                    }

                    UClass* ComponentClass = FindClassByName(ComponentJson["class"].get<FString>());
                    if (ComponentClass == nullptr)
                    {
                        throw std::runtime_error("Unknown component class: " + ComponentJson["class"].get<FString>());
                    }

                    UActorComponent* Component = Cast<UActorComponent>(FObjectFactory::ConstructObject(ComponentClass));
                    if (Component == nullptr)
                    {
                        throw std::runtime_error("Failed to create component instance.");
                    }

                    Component->SetOuter(Actor);
                    Component->RegisterComponent();
                    LoadContext.ObjectsBySavedId[ComponentJson["id"].get<uint32>()] = Component;
                    PendingComponents.push_back({Component, ComponentJson});
                }
            }
        }
        else if (j.contains("Primitives"))
        {
            throw std::runtime_error("Legacy primitive-only scene format is no longer supported by this loader.");
        }

        for (const auto& PendingActor : PendingActors)
        {
            AActor* Actor = PendingActor.Instance;
            const json& ActorJson = PendingActor.Data;

            if (ActorJson.contains("name"))
            {
                Actor->SetName(ActorJson["name"].get<FString>());
            }
            if (ActorJson.contains("active"))
            {
                Actor->SetActive(ActorJson["active"].get<bool>());
            }
            if (ActorJson.contains("properties"))
            {
                DeserializeReflectedProperties(Actor, ActorJson["properties"], LoadContext);
            }
        }

        for (const auto& PendingComponent : PendingComponents)
        {
            ApplyComponentJson(PendingComponent.Instance, PendingComponent.Data, LoadContext);
        }

        for (const auto& PendingActor : PendingActors)
        {
            AActor* Actor = PendingActor.Instance;
            const json& ActorJson = PendingActor.Data;

            if (ActorJson.contains("properties"))
            {
                ResolveReflectedProperties(Actor, ActorJson["properties"], LoadContext);
            }

            const json RootComponentId = ActorJson.value("root_component_id", json(nullptr));
            if (!RootComponentId.is_null())
            {
                auto It = LoadContext.ObjectsBySavedId.find(RootComponentId.get<uint32>());
                if (It != LoadContext.ObjectsBySavedId.end())
                {
                    if (USceneComponent* RootComponent = Cast<USceneComponent>(It->second))
                    {
                        Actor->SetRootComponent(RootComponent);
                    }
                }
            }
        }

        for (const auto& PendingComponent : PendingComponents)
        {
            ResolveComponentJson(PendingComponent.Instance, PendingComponent.Data, LoadContext);
        }

        if (!LoadContext.Errors.empty())
        {
            for (const FString& Error : LoadContext.Errors)
            {
                UImGuiManager::Get().AddLog("[SceneLoad] " + Error);
            }
        }

        if (OldLevel != nullptr)
        {
            Levels.erase(OldLevel); 
            delete OldLevel;
        }
    }
    catch (const std::exception& Exception)
    {
        Levels.erase(CurrentLevel);
        delete CurrentLevel;
        CurrentLevel = OldLevel;

        UImGuiManager::Get().AddLog(FString("[SceneLoad] Failed: ") + Exception.what());

        return false;
    }

    return true;
}

// 팩토리를 이용한 액터 동적 스폰 로직 구현
AActor *UWorld::SpawnActor(UClass *ClassToSpawn)
{
    // 1. 팩토리에게 신분증(UClass)을 주고 객체를 찍어내라고 명령합니다.
    UObject *NewObj = FObjectFactory::ConstructObject(ClassToSpawn);

    // 2. 만들어진 객체가 진짜 AActor가 맞는지 확인합니다.
    AActor *NewActor = Cast<AActor>(NewObj);

    if (NewActor != nullptr)
    {
        // 3. 레벨에 액터를 등록하고 초기화합니다.
        if (CurrentLevel)
        {
            NewActor->SetOuter(CurrentLevel);
            // 주의: ULevel의 GetActors()가 const 참조를 반환한다면,
            // Level 클래스 내부에 AddActor(AActor* InActor) 함수를 따로 만들어 호출하는 것을 권장합니다.
            CurrentLevel->GetActors().push_back(NewActor);
        }
        return NewActor;
    }

    return nullptr; // 팩토리 생성이 실패하거나 AActor가 아니면 nullptr 반환
}

bool UWorld::DeleteSelectedActors()
{
    if (CurrentLevel == nullptr || GEditor == nullptr || GEditor->GetSelection() == nullptr)
    {
        return false;
    }

    USelection* Selection = GEditor->GetSelection();
    if (Selection->IsEmpty())
    {
        return false;
    }

    TArray<AActor*> ActorsToDelete;
    const uint32 SelectionCount = Selection->GetCount();
    for (uint32 i = 0; i < SelectionCount; ++i)
    {
        UObject* SelectedObject = (*Selection)[i];
        if (SelectedObject == nullptr)
        {
            continue;
        }

        AActor* SelectedActor = Cast<AActor>(SelectedObject);
        if (SelectedActor == nullptr)
        {
            if (USceneComponent* SelectedSceneComponent = Cast<USceneComponent>(SelectedObject))
            {
                SelectedActor = SelectedSceneComponent->GetOwner();
            }
        }

        if (SelectedActor == nullptr)
        {
            continue;
        }

        if (std::find(ActorsToDelete.begin(), ActorsToDelete.end(), SelectedActor) == ActorsToDelete.end())
        {
            ActorsToDelete.push_back(SelectedActor);
        }
    }

    if (ActorsToDelete.empty())
    {
        return false;
    }

    Selection->Clear();

    TArray<AActor*>& LevelActors = CurrentLevel->GetActors();
    for (AActor* ActorToDelete : ActorsToDelete)
    {
        auto It = std::find(LevelActors.begin(), LevelActors.end(), ActorToDelete);
        if (It == LevelActors.end())
        {
            continue;
        }

        delete *It;
        LevelActors.erase(It);
    }

    return true;
}

void UWorld::Tick(float deltaTime)
{
    if (CurrentLevel)
    {
        for (AActor *actor : CurrentLevel->GetActors())
        {
            actor->Tick(deltaTime);
        }
    }
}

FHitResult UWorld::PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection)
{
    FHitResult ClosestHit;

    // 선택된 액터의 컴포넌트 중 가장 가까운 오브젝트 하나만 색출 
    // (시각적 선택 상태 갱신 로직, ImGuiManager 의존성 분리)
    if (CurrentLevel)
    {
        for (AActor *actor : CurrentLevel->GetActors())
        {
            for (UActorComponent *actorC : actor->GetOwnedComponents())
            {
                UPrimitiveComponent *Object = Cast<UPrimitiveComponent>(actorC);
                if (Object != nullptr)
                {
                    FHitResult Hit = Object->IntersectRay(RayOrigin, RayDirection);

                    // Ray와 가장 가까운 오브젝트 하나만 색출
                    if (Hit.bHit && Hit.Distance < ClosestHit.Distance)
                    {
                        ClosestHit = Hit;
                    }
                }
            }
        }
    }

    return ClosestHit;
}
