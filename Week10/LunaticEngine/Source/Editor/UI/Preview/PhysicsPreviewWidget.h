#pragma once
#include "AssetPreviewWidget.h"

// 물리 시뮬레이션을 확인하기 위한 Preview UI입니다.
// 시뮬레이션 시작/정지, 충돌체 표시, 리셋, Ragdoll 설정 등의 물리 전용 조작을 담당합니다.
// TODO: 각 Widget에 공통되는 사항을 FAssetPreviewWidget에 추상화
class FPhysicsPreviewWidget : public FAssetPreviewWidget
{
public:
	
};
