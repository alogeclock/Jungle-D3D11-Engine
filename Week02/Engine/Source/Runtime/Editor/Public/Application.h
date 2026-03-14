#pragma once  

#include <windows.h>  

#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"  
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"  
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/BoxComponent.h"  
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SphereComponent.h"  
#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"  
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
   HWND hWnd = nullptr;  

   FViewport* Viewport = nullptr;  
   URenderer* Renderer = nullptr;  

   // 임시  
   UAxisComponent* MainAxis;  
   ID3D11Buffer* VertexBuffer;  

   USphereComponent* sphere;  
   UINT numVerticesSphere;  
   ID3D11Buffer* vertexBufferSphere;  
};