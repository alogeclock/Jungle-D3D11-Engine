#pragma once

#include <windows.h>
#include <windowsx.h>

#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Engine/Source/Runtime/Editor/Public/Axis.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SphereComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/TriangleComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PlaneComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/TorusComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeArrowComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/RingComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"
#include "Engine/Source/Runtime/Editor/Public/PivotTransformGizmo.h"
#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"
#include "Engine/Source/Runtime/Core/Public/TimeManager.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/MeshManager.h"
#include "Engine/Source/Runtime/Editor/Public/Viewport.h"

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
    HWND      hWnd = nullptr;

    FViewport    *Viewport = nullptr;
    URenderer    *Renderer = nullptr;

    UAxis            *MainAxis = nullptr;
    UCubeComponent   *cube = nullptr;
    USphereComponent *sphere = nullptr;
    URingComponent   *ring = nullptr;

    APivotTransformGizmo *gizmo = nullptr;
};