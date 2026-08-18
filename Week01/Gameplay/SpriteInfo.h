#pragma once

#include <string>
#include <DirectXMath.h>

// 스프라이트 렌더링에 필요한 정보
// 텍스처 아틀라스 내에서 어떤 영역을 그릴지 지정
struct FSpriteInfo
{
	std::string TextureKey;                              // TextureManager에 등록된 키
	DirectX::XMFLOAT2 SpriteSize = { 0.0f, 0.0f };      // 스프라이트 크기 (픽셀, 0이면 텍스처 전체)
	DirectX::XMFLOAT2 SpriteOffset = { 0.0f, 0.0f };    // 아틀라스 내 오프셋 (픽셀)
	bool bIsMirrored = false;                            // 좌우 반전
};
