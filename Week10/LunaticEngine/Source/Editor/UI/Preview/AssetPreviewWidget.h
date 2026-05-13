#pragma once
#include "Editor/UI/Preview/PreviewViewportWidget.h"
#include "Editor/Viewport/Preview/PreviewViewportClient.h"
#include "Core/CoreTypes.h"
#include "Math/Matrix.h"

#include <functional>
#include <memory>

class FViewport;
class UEditorEngine;
struct ID3D11Device;

// Asset Preview 창의 베이스 UI 클래스입니다.
// 뷰포트 이미지 출력, 공통 툴바, 입력 포커스 처리를 담당합니다.
class FAssetPreviewWidget
{
public:
	virtual ~FAssetPreviewWidget();
	
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
	
	// 공통 접근, 구체 ViewportClient 접근이 필요할 때는 하위 클래스에서 Typed Getter 사용
	std::unique_ptr<FPreviewViewportClient> PreviewViewportClient = nullptr;
	
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
	virtual std::unique_ptr<FPreviewViewportClient> CreatePreviewViewportClient();
	FPreviewViewportClient* EnsurePreviewViewportClient();
	FPreviewViewportClient* GetPreviewViewportClient() const { return PreviewViewportClient.get(); }
	void ReleasePreviewViewportClient();
	void RegisterPreviewClient();
	void UnregisterPreviewClient();
	
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
