#pragma once

#include "Rendering/Renderer.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SphereComponent.h"

class UImGuiManager
{
public:
	static UImGuiManager& Get()
	{
		static UImGuiManager instance;
		return instance;
	}

	UImGuiManager() = default;
	~UImGuiManager() = default;

public:
	void Create(HWND hWnd, URenderer* renderer);
	void Update();
	void beginFrame();
	void endFrame();
	void Release();

	void SetSelectedObject(USphereComponent* sphere);

private:
	USphereComponent* SelectedObject;
	char buffer[256];
};