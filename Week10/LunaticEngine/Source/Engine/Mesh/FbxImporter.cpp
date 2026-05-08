#include "FbxImporter.h"

#include <fbxsdk.h>

bool FFbxImporter::TestLoadScene(const FString& SourcePath)
{
    FbxManager* Manager = FbxManager::Create();
    if (!Manager)
    {
        return false;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IOSettings);

    FbxImporter* Importer = FbxImporter::Create(Manager, "");
    if (!Importer)
    {
        Manager->Destroy();
        return false;
    }

    // FString 변환은 현재 엔진 문자열 타입에 맞게 조정 필요
    const std::string NarrowPath(SourcePath.begin(), SourcePath.end());

    const bool bInitialized = Importer->Initialize(NarrowPath.c_str(), -1, Manager->GetIOSettings());

    if (!bInitialized)
    {
        Importer->Destroy();
        Manager->Destroy();
        return false;
    }

    FbxScene* Scene = FbxScene::Create(Manager, "ImportScene");
    if (!Scene)
    {
        Importer->Destroy();
        Manager->Destroy();
        return false;
    }

    const bool bImported = Importer->Import(Scene);

    Importer->Destroy();
    Manager->Destroy();

    return bImported;
}
