#pragma once

#include "Engine/Source/Runtime/Editor/Public/Application.h"
#include "Engine/Source/Runtime/Editor/Public/EditorViewportClient.h"

// 사용자의 Editor Input을 받는 곳

class FViewport
{
public:
	FViewport();
	~FViewport();

public:
	void CreateEditorViewportClient();

    // WndProc에서 호출 — 키/마우스 상태 업데이트
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

    // 매 프레임 Application 메인루프에서 호출
    void Tick(float DeltaTime);

private:
    //FEditorViewportClient* ViewportClient;

    std::unordered_map<uint32, bool> KeyStates;

    int32 MouseX = 0;
    int32 MouseY = 0;
    int32 PrevMouseX = 0;
    int32 PrevMouseY = 0;
    int32 MouseDeltaX = 0;
    int32 MouseDeltaY = 0;
};