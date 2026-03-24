#pragma once

#include "CoreTypes.h"
#include "Source/Engine/Object/Public/Object.h"
#include "Source/Engine/Public/GUI/ImGuiManager.h"
#include "Source/Core/Public/Math/Vector.h"

class UPrimitiveComponent;
class APivotTransformGizmo;
class AGrid;
class AAxis;
class FEditorViewportClient;
class FViewport;

struct FKey
{
    uint32 KeyCode;
    explicit FKey(uint32 InCode) : KeyCode(InCode) {}
    bool operator==(const FKey &O) const { return KeyCode == O.KeyCode; }
};

namespace EKeys
{
    inline const FKey W{'W'};
    inline const FKey A{'A'};
    inline const FKey S{'S'};
    inline const FKey D{'D'};
    inline const FKey Q{'Q'};
    inline const FKey E{'E'};
    inline const FKey Space{VK_SPACE};
    inline const FKey LeftMouseButton{VK_LBUTTON};
    inline const FKey RightMouseButton{VK_RBUTTON};
} // namespace EKeys

enum class EInputEvent : uint8
{
    Pressed = 0,
    Released = 1,
    Repeat = 2,
    Axis = 3,
};

struct FInputEventState
{
  public:
    FInputEventState(FViewport *InViewport, FKey InKey, EInputEvent InInputEvent) : Viewport(InViewport), Key(InKey), InputEvent(InInputEvent) {}

    FViewport  *GetViewport() const { return Viewport; }
    EInputEvent GetInputEvent() const { return InputEvent; }
    FKey        GetKey() const { return Key; }

    bool IsLeftMouseButtonPressed() const;
    bool IsRightMouseButtonPressed() const;
    bool IsButtonPressed(FKey InKey) const;

  private:
    /** Viewport the event was sent to */
    FViewport *Viewport;
    /** Pressed Key */
    FKey Key;
    /** Key event */
    EInputEvent InputEvent;
};

// 입력 이벤트를 수신할 리스너 인터페이스 (기즈모가 상속받는 구조체)
class IViewportInputListener
{
public:
    virtual bool OnMouseDown(const FVector<float>& RayOrigin, const FVector<float>& RayDir) { return false; }
    virtual void OnMouseMove(const FVector<float>& RayOrigin, const FVector<float>& RayDir) {}
    virtual void OnMouseHover(const FVector<float>& RayOrigin, const FVector<float>& RayDir) {}
    virtual void OnMouseUp() {}
    virtual bool OnKeyDown(const FKey& Key) { return false; }
};

// 선택된 객체(Object)들을 관리하는 전용 컨테이너
class USelection
{
public:
    void Clear();

    void AddObject(UObject* InObject);
    bool IsSelected(UObject* InObject) const;
    void RemoveObject(UObject* InObject);

    UObject* GetSelectedObject(int32 Index = 0) const;
    uint32 GetCount() const { return SelectedObjects.size(); }
    bool IsEmpty() const;
    UObject* operator[](uint32 index);

private:
    TArray<UObject*> SelectedObjects;
};

// 에디터의 최상위 베이스 클래스 (사용자 입력, 선택된 오브젝트 관리)
class UEditorEngine : public UObject
{
public:
	UEditorEngine(const FString& InString);
    virtual ~UEditorEngine();

    void Tick(float DeltaTime);
    
    void SetEditorViewportClient(FEditorViewportClient* InViewportClient) { ViewportClient = InViewportClient; }
    FEditorViewportClient* GetEditorViewportClient() { return ViewportClient; };

    USelection* GetSelection() const { return Selection; }
    TArray<IViewportInputListener*>* GetInputListeners() { return &InputListeners; };

    // 입력 의존성 극복을 위한 리스너 등록/해제
    void RegisterInputListener(IViewportInputListener* Listener);
    void UnregisterInputListener(IViewportInputListener* Listener);

    // ⭐사용자 입력을 가로채서 분석 및 라우팅
    bool ProcessMouseDown(const FVector<float>& RayOrigin, const FVector<float>& RayDir);
    void ProcessMouseMove(const FVector<float>& RayOrigin, const FVector<float>& RayDir);
    void ProcessMouseHover(const FVector<float>& RayOrigin, const FVector<float>& RayDir);
    void ProcessMouseUp();
    bool ProcessKeyDown(const FKey& Key);

    // 입력 분석 후 현재 선택한 오브젝트를 Selection 배열에 삽입
    void UpdateSelection(UPrimitiveComponent* HitComp);
    APivotTransformGizmo* GetGizmo() const { return Gizmo; }
    AGrid* GetGrid() { return Grid; }
    AAxis* GetAxis() { return Axis; }

protected:
    USelection* Selection;
    TArray<IViewportInputListener*> InputListeners;

    FEditorViewportClient* ViewportClient = nullptr;
    AGrid* Grid = nullptr;
    AAxis* Axis = nullptr;
    APivotTransformGizmo* Gizmo = nullptr;
};

extern UEditorEngine* GEditor;