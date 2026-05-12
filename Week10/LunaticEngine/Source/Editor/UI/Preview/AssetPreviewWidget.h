#pragma once
#include "Editor/UI/Preview/PreviewViewportWidget.h"

class UEditorEngine;

// Asset Preview 창의 베이스 UI 클래스입니다.
// 뷰포트 이미지 출력, 공통 툴바, 입력 포커스 처리를 담당합니다.
class FAssetPreviewWidget
{
protected:
	UEditorEngine* Engine = nullptr;
	FPreviewViewportWidget ViewportWidget;
	
private:
};
