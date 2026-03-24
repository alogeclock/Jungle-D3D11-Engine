#pragma once

#include "Source/Editor/Public/Application.h"
#include "Source/Editor/Public/EditorViewportClient.h"

// 사용자의 Editor Input을 받는 곳

class FEditorViewportClient;

class FViewport
{
public:
	FViewport();
	~FViewport();

public:
    // WndProc에서 호출 ? 키/마우스 상태 업데이트
    void OnKeyDown(uint32 KeyCode);
    void OnKeyUp(uint32 KeyCode);
    void OnMouseMove(int32 X, int32 Y);
    void OnMouseButtonDown(uint32 KeyCode, int32 X, int32 Y);
    void OnMouseButtonUp(uint32 KeyCode);
    void OnMouseWheel(float Delta);

    // FInputEventState에서 사용
    bool KeyState(struct FKey InKey) const;

    // 마우스 현재 위치
    int32 GetMouseX() const { return MouseX; }
    int32 GetMouseY() const { return MouseY; }
    int32 GetMouseDeltaX() const { return MouseDeltaX; }
    int32 GetMouseDeltaY() const { return MouseDeltaY; }

    FEditorViewportClient* GetViewportClient() const { return ViewportClient; }
    void SetEditorViewportClient(FEditorViewportClient *InViewportClient) { ViewportClient = InViewportClient; }

    void OnResize(uint32 NewWidth, uint32 NewHeight)
    {
        Width = NewWidth;
        Height = NewHeight;
    }

    uint32 GetWidth() const { return Width; }
    uint32 GetHeight() const { return Height; }

    uint32 SetWidth(uint32 width) { return Width = width; }
    uint32 SetHeight(uint32 height) { return Height = height; }

private:
    FEditorViewportClient* ViewportClient; // ViewportClient를 갖고 있지만 단순히 참조만 한다.

    std::unordered_map<uint32, bool> KeyStates;

    int32 MouseX = 0;
    int32 MouseY = 0;
    int32 PrevMouseX = 0;
    int32 PrevMouseY = 0;
    int32 MouseDeltaX = 0;
    int32 MouseDeltaY = 0;

    uint32 Width = 0.0f;
    uint32 Height = 0.0f;
};