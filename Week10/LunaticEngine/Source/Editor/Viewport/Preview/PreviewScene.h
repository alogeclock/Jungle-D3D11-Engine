#pragma once
#include "GameFramework/World.h"

// 에디터 월드와 분리된 프리뷰를 위한 전용 월드/환경 컨테이너 - UE5 PreviewScene, 실제 렌더 씬 구현체인 FScene과는 구분됨
// 프리뷰 액터, 기본 조명, Tick 수명주기를 한곳에서 관리합니다.
class FPreviewScene
{
public:
	UWorld* GetWorld() const { return PreviewWorld; }
	FScene& GetScene() const { return PreviewWorld->GetScene(); }
	
	void Initialize();
	void Release();
	void Tick(float DeltaTime);
	void Clear();

	AActor* SpawnPreviewActor();
	void DestroyPreviewActor(AActor* Actor);
	
private:
	UWorld* PreviewWorld = nullptr;
	TArray<AActor*> PreviewActors;
};
