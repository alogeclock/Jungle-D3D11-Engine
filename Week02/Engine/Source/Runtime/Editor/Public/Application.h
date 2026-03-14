#pragma once

#include <windows.h>

#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"
#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"

#include <iostream>

class UApplication
{
public: 
	UApplication();
	~UApplication();

public:
	void Initialize(HINSTANCE hInstance);
	void Run();
	void Finish();

private:
	HINSTANCE hInst = nullptr;
	HWND hWnd = nullptr;
	URenderer* Renderer = nullptr;

	// 임시
	UAxisComponent* MainAxis;
	ID3D11Buffer* VertexBuffer;

	USphere* sphere;
	UINT numVerticesSphere;
	ID3D11Buffer* vertexBufferSphere;
};