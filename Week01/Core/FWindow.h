#pragma once

#include <functional>
#include <windows.h>

class FWindow
{
  public:
    bool Initialize(HINSTANCE InInstance, int InWidth, int InHeight, const wchar_t *InTitle);
    void Shutdown();

    bool ProcessMessages();
    HWND GetHandle() const;

    void SetResizeCallback(std::function<void(int, int)> Callback) { OnResize = std::move(Callback); }

  private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    LRESULT                 HandleMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

  private:
    HINSTANCE InstanceHandle = nullptr;
    HWND      WindowHandle = nullptr;
    int       Width = 0;
    int       Height = 0;
    bool      bShouldClose = false;

    std::function<void(int, int)> OnResize;
};
