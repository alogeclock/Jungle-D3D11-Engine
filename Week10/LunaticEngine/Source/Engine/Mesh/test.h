#pragma once
#include "fbxsdk.h"

void TestFbxSdkLink()
{
	const char* lFilename = "test.fbx";
	FbxManager* Manager = FbxManager::Create();
	FbxIOSettings* Ios = FbxIOSettings::Create(Manager, IOSROOT);
	if (Manager)
	{
		Manager->Destroy();
	}
}