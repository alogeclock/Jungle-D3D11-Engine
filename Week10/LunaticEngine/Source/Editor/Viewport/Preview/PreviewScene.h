#pragma once
#include "GameFramework/World.h"

// 에디터 월드와 분리된 프리뷰 전용 월드/환경을 소유하는 씬입니다.
// 프리뷰 액터, 기본 조명, Tick 수명주기를 한곳에서 관리합니다.
class FPreviewScene
{
public:
	UWorld* GetWorld() const { return PreviewWorld; }
	FScene& GetScene() const { return PreviewWorld->GetScene(); }
	
	void Tick(float DeltaTime);
	void Clear();
	
private:
	UWorld* PreviewWorld = nullptr;
	TArray<AActor> PreviewActors;
};
