#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>

struct FFbxSceneHandle
{
    FbxManager* Manager = nullptr;
    FbxScene*   Scene   = nullptr;

    // 소유 중인 FBX manager와 scene 리소스를 해제한다.
    ~FFbxSceneHandle();

    FFbxSceneHandle()                                  = default;
    FFbxSceneHandle(const FFbxSceneHandle&)            = delete;
    FFbxSceneHandle& operator=(const FFbxSceneHandle&) = delete;
};

class FFbxSceneLoader
{
public:
    // FBX 파일을 열어 manager와 scene handle을 생성한다.
    static bool Load(const FString& SourcePath, FFbxSceneHandle& OutScene, FString* OutMessage = nullptr);

    // scene의 모든 mesh geometry를 삼각형으로 변환한다.
    static void Triangulate(FbxManager* Manager, FbxScene* Scene);

    // scene의 axis system과 unit을 엔진 기준으로 정규화한다.
    static void Normalize(FbxScene* Scene);
};
