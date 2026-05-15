#pragma once
#include "AssetEditorWidget.h"

class FAnimSequenceEditorWidget : public FAssetEditorWidget
{
public:
	FAnimSequenceEditorWidget() = default;

	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Tick(float DeltaTime) override;
	void Render(float DeltaTime) override;
	void Close() override;

private:
	// TODO: Anim Sequence에 필요한 멤버 변수들을 넣는다
};

