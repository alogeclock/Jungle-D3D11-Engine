#pragma once
#include "Editor/UI/Preview/PreviewViewportWidget.h"
#include "Core/CoreTypes.h"
#include "Math/Matrix.h"

#include <functional>

class FPreviewViewportClient;
class FViewport;
class UEditorEngine;
struct ID3D11Device;

// Asset Preview 창의 베이스 UI 클래스입니다.
// 뷰포트 이미지 출력, 공통 툴바, 입력 포커스 처리를 담당합니다.
class FAssetPreviewWidget
{
public:
	virtual ~FAssetPreviewWidget() = default;
	
	virtual void Shutdown() {}
	virtual void Render(float DeltaTime) { (void)DeltaTime; }

protected:
	UEditorEngine* Engine = nullptr;
	FPreviewViewportWidget ViewportWidget;
	FViewport* Viewport = nullptr;
	ID3D11Device* Device = nullptr;
	
	int32 EditorInstanceId = 0;
	uint32 PreviewDockId = 0;
	
	bool bOpen = false;
	bool bRegistered = false;
	bool bCapturingInput = false;
	
private:
	FPreviewViewportClient* PreviewViewportClient = nullptr;
	
	// ─────────────────────── AssetPreviewWidget 자식 클래스 공통 함수 ───────────────────────
public:
	// Input
	void ClearInputCapture();
	
	// Getter & Setter
	void SetDockId(uint32 InDockId) { PreviewDockId = InDockId; }
	bool IsOpen() const { return bOpen; }
	bool IsCapturingInput() const { return bCapturingInput; }
	
protected:
	// Preview ViewportClient Register/Unregister
	void SetPreviewViewportClient(FPreviewViewportClient* InViewportClient);
	void RegisterPreviewClient(FPreviewViewportClient* InViewportClient);
	void UnregisterPreviewClient(FPreviewViewportClient* InViewportClient);
	
	// Preview Widget 전용 Window
	bool IsMultiViewportEnabled() const;
	void SetNextPreviewEditorWindowPolicy() const;
	
	// UI & Editor Rendering
	bool BeginPreviewDetailsSection(const char* SectionName) const;
	bool DrawPreviewLabeledField(const char* Label, const std::function<bool()>& DrawField) const;
	void DrawPreviewReadOnlyField(const char* Label, const FString& Value) const;
	bool DrawPreviewColoredFloat3(const char* Label, float Values[3], float Speed, const float* ResetValues = nullptr) const;
	void DrawTransformMatrixRows(const char* SectionName, const FMatrix& Matrix) const;
	
	// Text Formatting
	FString FormatPreviewVector(const FVector& Value) const;

	static constexpr float PreviewDetailsPropertyLabelWidth = 124.0f;
	static constexpr float PreviewDetailsPropertyVerticalSpacing = 6.0f;
};
