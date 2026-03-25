#pragma once

#include <windows.h>
#include <windowsx.h>

#include "Source/Core/Public/TimeManager.h"
#include "Source/Editor/Public/Axis.h"
#include "Source/Editor/Public/EditorEngine.h"
#include "Source/Editor/Public/Grid.h"
#include "Source/Editor/Public/PivotTransformGizmo.h"
#include "Source/Editor/Public/Viewport.h"
#include "Source/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Source/Engine/Public/Classes/Components/AxisComponent.h"
#include "Source/Engine/Public/Classes/Components/CubeArrowComponent.h"
#include "Source/Engine/Public/Classes/Components/CubeComponent.h"
#include "Source/Engine/Public/Classes/Components/GridComponent.h"
#include "Source/Engine/Public/Classes/Components/PlaneComponent.h"
#include "Source/Engine/Public/Classes/Components/RingComponent.h"
#include "Source/Engine/Public/Classes/Components/SphereComponent.h"
#include "Source/Engine/Public/Classes/Components/TriangleComponent.h"
#include "Source/Engine/Public/Classes/MeshManager.h"
#include "Source/Engine/Public/Classes/TextureManager.h"
#include "Source/Engine/Public/GUI/ImGuiManager.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include "Source/Engine/Public/Rendering/Scene.h"

#include <iostream>

extern UWorld* GWorld;
extern UEditorEngine* GEditor;
extern FScene* GMainScene;

class UApplication
{
public:
    UApplication();
    ~UApplication();

public:
    void Initialize(HINSTANCE hInstance);
    void Run();
    void Tick(float DeltaTime);
    void Render();
    void Finish();

    void OnResize(uint32 NewWidth, uint32 NewHeight);
    FViewport* GetViewport()
    {
        return Viewport;
    }

    void UpdateEditorViewport();

private:
    HINSTANCE hInst = nullptr;
    HWND hWnd = nullptr;

    FViewport* Viewport = nullptr;
    URenderer* Renderer = nullptr;

    bool bResize = false;
    uint32 Width = 0.0f;
    uint32 Height = 0.0f;
};

extern UApplication* GApplication;