#pragma once

#include "PreviewViewportClient.h"

// 애니메이션 프리뷰 뷰포트의 입력, 카메라, 재생 상태를 다루는 클라이언트입니다.
// 공통 프리뷰 카메라 조작 위에 애니메이션 재생 로직을 얹기 위한 확장 지점입니다.
class FAnimationPreviewViewportClient : public FPreviewViewportClient
{
public:
	FAnimationPreviewViewportClient() = default;
	~FAnimationPreviewViewportClient() override = default;
};
