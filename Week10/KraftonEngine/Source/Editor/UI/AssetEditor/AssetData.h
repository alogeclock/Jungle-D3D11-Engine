#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Object/Object.h"

class FArchive;

// ============================================================
// UAssetData
// ------------------------------------------------------------
// Editor에서 .uasset 파일을 열었을 때 역직렬화되는 공통 루트 타입.
// 현재 1차 구현은 Camera Shake Stack 전용 데이터를 지원한다.
// ============================================================
class UAssetData : public UObject
{
public:
	DECLARE_CLASS(UAssetData, UObject)

	void Serialize(FArchive& Ar) override;
};

// 모든 CameraModifier가 공통으로 저장하는 설정값.
// CameraOwner, Alpha, ElapsedTime 같은 런타임 상태는 저장하지 않는다.
struct FCameraModifierCommonAssetDesc
{
	uint8 Priority = 128;
	float AlphaInTime = 0.05f;
	float AlphaOutTime = 0.10f;
	bool bStartDisabled = false;
};

// ImGui issue #786의 cubic Bezier 위젯 형식과 동일하게 저장한다.
// P0=(0,0), P3=(1,1)은 고정이고, P1/P2만 4개 float로 저장한다.
struct FAssetBezierCurve
{
	float ControlPoints[4] = { 0.25f, 0.0f, 0.75f, 1.0f };

	float Evaluate(float NormalizedTime) const;
	void ResetLinear();
	void Serialize(FArchive& Ar);
};

struct FCameraShakePatternCurves
{
	FAssetBezierCurve TranslationX;
	FAssetBezierCurve TranslationY;
	FAssetBezierCurve TranslationZ;
	FAssetBezierCurve RotationX;
	FAssetBezierCurve RotationY;
	FAssetBezierCurve RotationZ;

	void ResetLinear();
	void CopyFrom(const FAssetBezierCurve& SourceCurve);
	void Serialize(FArchive& Ar);
};

struct FCameraShakeModifierAssetDesc
{
	uint64 EditorId = 0;
	FString Name = "CameraShake";

	FCameraModifierCommonAssetDesc Common;

	float Duration = 0.5f;
	float Intensity = 1.0f;
	float Frequency = 20.0f;

	FVector LocationAmplitude = FVector(0.0f, 0.0f, 5.0f);
	FRotator RotationAmplitude = FRotator(1.0f, 1.0f, 0.0f);

	bool bUseCurves = true;
	FCameraShakePatternCurves Curves;
};

// ============================================================
// UCameraModifierStackAssetData
// ------------------------------------------------------------
// CameraShakeStack.uasset의 루트 UObject.
// 지금은 Camera Shake 리스트만 저장하지만, 이후 FOV/Offset 등을 추가해도
// 같은 Asset Editor/Details 흐름을 유지할 수 있다.
// ============================================================
class UCameraModifierStackAssetData : public UAssetData
{
public:
	DECLARE_CLASS(UCameraModifierStackAssetData, UAssetData)

	TArray<FCameraShakeModifierAssetDesc> CameraShakes;

	void Serialize(FArchive& Ar) override;
	void EnsureValidEditorIds();
};

void SerializeCameraModifierCommonAssetDesc(FArchive& Ar, FCameraModifierCommonAssetDesc& Desc);
void SerializeCameraShakeModifierAssetDesc(FArchive& Ar, FCameraShakeModifierAssetDesc& Desc);
uint64 GenerateAssetEditorId();
