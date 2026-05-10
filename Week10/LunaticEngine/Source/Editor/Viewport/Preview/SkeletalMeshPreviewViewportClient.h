#pragma once

#include "PreviewViewportClient.h"

class USkeletalMesh;
class USkeletalMeshComponent;

// 특정 스켈레탈 메시를 독립된 프리뷰 환경(Preview Scene)에 렌더링하고 제어합니다.
// 메시 교체, 카메라 포커스 맞춤, 본 구조 시각화 등 스켈레탈 메시 에디터의 핵심 뷰포트 로직을 담당합니다.
class FSkeletalMeshPreviewViewportClient : public FPreviewViewportClient
{
public:
	FSkeletalMeshPreviewViewportClient();
	~FSkeletalMeshPreviewViewportClient() override;
	
	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
	USkeletalMesh* GetSkeletalMesh() const { return PreviewSkeletalMesh; }
	USkeletalMeshComponent* GetPreviewComponent() const { return PreviewComponent; }

	bool FocusPreviewMesh();

protected:
	void OnCameraReset() override;

private:
	void CreatePreviewComponent();
	void DestroyPreviewComponent();

private:
	USkeletalMesh* PreviewSkeletalMesh = nullptr;
	USkeletalMeshComponent* PreviewComponent = nullptr;
};
