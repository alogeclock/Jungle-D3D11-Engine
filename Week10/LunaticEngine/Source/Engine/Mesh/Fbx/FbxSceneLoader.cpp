#include "Mesh/Fbx/FbxSceneLoader.h"

#include "Engine/Platform/Paths.h"

#include <fbxsdk.h>

namespace
{
    // FString error 출력 대상이 있을 때만 메시지를 기록한다.
    void SetMessage(FString* OutMessage, const FString& Message)
    {
        if (OutMessage)
        {
            *OutMessage = Message;
        }
    }

    // C 문자열 error 출력 대상이 있을 때만 메시지를 기록한다.
    void SetMessage(FString* OutMessage, const char* Message)
    {
        if (OutMessage)
        {
            *OutMessage = Message ? Message : "";
        }
    }
}

// 소유 중인 FBX manager와 scene 리소스를 해제한다.
FFbxSceneHandle::~FFbxSceneHandle()
{
    if (Manager)
    {
        Manager->Destroy();
        Manager = nullptr;
        Scene   = nullptr;
    }
}

// FBX 파일을 열어 manager와 scene handle을 생성한다.
bool FFbxSceneLoader::Load(const FString& SourcePath, FFbxSceneHandle& OutScene, FString* OutMessage)
{
    const FString FullPath = FPaths::ConvertRelativePathToFull(SourcePath);

    FbxManager* Manager = FbxManager::Create();
    if (Manager == nullptr)
    {
        SetMessage(OutMessage, "Failed to create FBX manager");
        return false;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
    if (IOSettings == nullptr)
    {
        SetMessage(OutMessage, "Failed to create FBX IO settings");
        Manager->Destroy();
        return false;
    }

    IOSettings->SetBoolProp(IMP_FBX_MATERIAL, true);
    IOSettings->SetBoolProp(IMP_FBX_TEXTURE, true);
    IOSettings->SetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true);

    Manager->SetIOSettings(IOSettings);

    FbxImporter* Importer = FbxImporter::Create(Manager, "");
    if (Importer == nullptr)
    {
        SetMessage(OutMessage, "Failed to create FBX importer");
        Manager->Destroy();
        return false;
    }

    if (!Importer->Initialize(FullPath.c_str(), -1, Manager->GetIOSettings()))
    {
        FString Error = "FBX initialize failed: ";
        Error         += Importer->GetStatus().GetErrorString();

        SetMessage(OutMessage, Error);

        Importer->Destroy();
        Manager->Destroy();
        return false;
    }

    FbxScene* Scene = FbxScene::Create(Manager, "ImportScene");
    if (!Scene)
    {
        SetMessage(OutMessage, "Failed to create FBX scene");
        Importer->Destroy();
        Manager->Destroy();
        return false;
    }

    if (!Importer->Import(Scene))
    {
        FString Error = "FBX import failed: ";
        Error         += Importer->GetStatus().GetErrorString();

        SetMessage(OutMessage, Error);

        Importer->Destroy();
        Manager->Destroy();
        return false;
    }

    Importer->Destroy();

    OutScene.Manager = Manager;
    OutScene.Scene   = Scene;
    return true;
}

// scene의 모든 mesh geometry를 삼각형으로 변환한다.
void FFbxSceneLoader::Triangulate(FbxManager* Manager, FbxScene* Scene)
{
    if (!Manager || !Scene)
    {
        return;
    }

    FbxGeometryConverter Converter(Manager);
    Converter.Triangulate(Scene, true);
}

// scene의 axis system과 unit을 엔진 기준으로 정규화한다.
void FFbxSceneLoader::Normalize(FbxScene* Scene)
{
    if (!Scene)
    {
        return;
    }

    FbxAxisSystem EngineAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded);

    const FbxAxisSystem SceneAxisSystem = Scene->GetGlobalSettings().GetAxisSystem();

    if (SceneAxisSystem != EngineAxisSystem)
    {
        EngineAxisSystem.ConvertScene(Scene);
    }

    const FbxSystemUnit EngineUnit = FbxSystemUnit::cm;
    const FbxSystemUnit SceneUnit  = Scene->GetGlobalSettings().GetSystemUnit();

    if (SceneUnit != EngineUnit)
    {
        EngineUnit.ConvertScene(Scene);
    }
}
