#include "Editor/Settings/PreviewSettings.h"

#include "Math/MathUtils.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
namespace Key
{
	constexpr const char* Viewport = "Viewport";
	constexpr const char* CameraSpeed = "CameraSpeed";
	constexpr const char* CameraRotationSpeed = "CameraRotationSpeed";
	constexpr const char* CameraZoomSpeed = "CameraZoomSpeed";
	constexpr const char* InitViewPos = "InitViewPos";
	constexpr const char* InitLookAt = "InitLookAt";

	constexpr const char* TransformTools = "TransformTools";
	constexpr const char* CoordSystem = "CoordSystem";
	constexpr const char* bEnableTranslationSnap = "bEnableTranslationSnap";
	constexpr const char* TranslationSnapSize = "TranslationSnapSize";
	constexpr const char* bEnableRotationSnap = "bEnableRotationSnap";
	constexpr const char* RotationSnapSize = "RotationSnapSize";
	constexpr const char* bEnableScaleSnap = "bEnableScaleSnap";
	constexpr const char* ScaleSnapSize = "ScaleSnapSize";

	constexpr const char* RenderOptions = "RenderOptions";
	constexpr const char* ViewMode = "ViewMode";
	constexpr const char* ViewportType = "ViewportType";
	constexpr const char* bPrimitives = "bPrimitives";
	constexpr const char* bSkeletalMesh = "bSkeletalMesh";
	constexpr const char* bGrid = "bGrid";
	constexpr const char* bWorldAxis = "bWorldAxis";
	constexpr const char* bGizmo = "bGizmo";
	constexpr const char* bBillboardText = "bBillboardText";
	constexpr const char* bBoundingVolume = "bBoundingVolume";
	constexpr const char* bDebugDraw = "bDebugDraw";
	constexpr const char* bSceneBVH = "bSceneBVH";
	constexpr const char* bOctree = "bOctree";
	constexpr const char* bWorldBound = "bWorldBound";
	constexpr const char* bLightVisualization = "bLightVisualization";
	constexpr const char* bLightHitMap = "bLightHitMap";
	constexpr const char* bFog = "bFog";
	constexpr const char* bFXAA = "bFXAA";
	constexpr const char* bViewLightCulling = "bViewLightCulling";
	constexpr const char* bVisualize25DCulling = "bVisualize25DCulling";
	constexpr const char* bShowShadowFrustum = "bShowShadowFrustum";
	constexpr const char* bGammaCorrection = "bGammaCorrection";
	constexpr const char* bSelectionOutline = "bSelectionOutline";
	constexpr const char* GridSpacing = "GridSpacing";
	constexpr const char* GridHalfLineCount = "GridHalfLineCount";
	constexpr const char* GridLineThickness = "GridLineThickness";
	constexpr const char* GridMajorLineThickness = "GridMajorLineThickness";
	constexpr const char* GridMajorLineInterval = "GridMajorLineInterval";
	constexpr const char* GridMinorIntensity = "GridMinorIntensity";
	constexpr const char* GridMajorIntensity = "GridMajorIntensity";
	constexpr const char* GridAxisThickness = "GridAxisThickness";
	constexpr const char* GridAxisIntensity = "GridAxisIntensity";
	constexpr const char* CameraMoveSensitivity = "CameraMoveSensitivity";
	constexpr const char* CameraRotateSensitivity = "CameraRotateSensitivity";
	constexpr const char* DisplayGamma = "DisplayGamma";
	constexpr const char* GammaCorrectionBlend = "GammaCorrectionBlend";
	constexpr const char* bUseSRGBCurve = "bUseSRGBCurve";
	constexpr const char* DebugLineThickness = "DebugLineThickness";
	constexpr const char* ActorHelperBillboardScale = "ActorHelperBillboardScale";
	constexpr const char* DirectionalLightVisualizationScale = "DirectionalLightVisualizationScale";
	constexpr const char* PointLightVisualizationScale = "PointLightVisualizationScale";
	constexpr const char* SpotLightVisualizationScale = "SpotLightVisualizationScale";
}

json::JSON MakeVectorJson(const FVector& Value);
void LoadVector(json::JSON& Object, const char* KeyName, FVector& OutValue);
void SaveRenderOptions(json::JSON& Object, const FViewportRenderOptions& Options);
void LoadRenderOptions(json::JSON& Object, FViewportRenderOptions& Options);
}

FString FPreviewSettings::GetDefaultSettingsPath()
{
	return FPaths::ToUtf8(FPaths::Combine(FPaths::SettingsDir(), L"PreviewViewport.ini"));
}

void FPreviewSettings::SaveToFile(const FString& Path) const
{
	using namespace json;

	JSON Root = Object();

	JSON Viewport = Object();
	Viewport[Key::CameraSpeed] = CameraSpeed;
	Viewport[Key::CameraRotationSpeed] = CameraRotationSpeed;
	Viewport[Key::CameraZoomSpeed] = CameraZoomSpeed;
	Viewport[Key::InitViewPos] = MakeVectorJson(InitViewPos);
	Viewport[Key::InitLookAt] = MakeVectorJson(InitLookAt);
	Root[Key::Viewport] = Viewport;

	JSON TransformTools = Object();
	TransformTools[Key::CoordSystem] = static_cast<int32>(CoordSystem);
	TransformTools[Key::bEnableTranslationSnap] = bEnableTranslationSnap;
	TransformTools[Key::TranslationSnapSize] = TranslationSnapSize;
	TransformTools[Key::bEnableRotationSnap] = bEnableRotationSnap;
	TransformTools[Key::RotationSnapSize] = RotationSnapSize;
	TransformTools[Key::bEnableScaleSnap] = bEnableScaleSnap;
	TransformTools[Key::ScaleSnapSize] = ScaleSnapSize;
	Root[Key::TransformTools] = TransformTools;

	JSON RenderOptionsObject = Object();
	SaveRenderOptions(RenderOptionsObject, RenderOptions);
	Root[Key::RenderOptions] = RenderOptionsObject;

	std::filesystem::path FilePath(FPaths::ToWide(Path));
	if (FilePath.has_parent_path())
	{
		std::filesystem::create_directories(FilePath.parent_path());
	}

	std::ofstream File(FilePath);
	if (File.is_open())
	{
		File << Root;
	}
}

void FPreviewSettings::LoadFromFile(const FString& Path)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(Content);

	if (Root.hasKey(Key::Viewport))
	{
		JSON Viewport = Root[Key::Viewport];
		if (Viewport.hasKey(Key::CameraSpeed))
		{
			CameraSpeed = Clamp(static_cast<float>(Viewport[Key::CameraSpeed].ToFloat()), 0.1f, 1000.0f);
		}
		if (Viewport.hasKey(Key::CameraRotationSpeed))
		{
			CameraRotationSpeed = Clamp(static_cast<float>(Viewport[Key::CameraRotationSpeed].ToFloat()), 0.1f, 1000.0f);
		}
		if (Viewport.hasKey(Key::CameraZoomSpeed))
		{
			CameraZoomSpeed = Clamp(static_cast<float>(Viewport[Key::CameraZoomSpeed].ToFloat()), 0.1f, 10000.0f);
		}
		LoadVector(Viewport, Key::InitViewPos, InitViewPos);
		LoadVector(Viewport, Key::InitLookAt, InitLookAt);
	}

	if (Root.hasKey(Key::TransformTools))
	{
		JSON TransformTools = Root[Key::TransformTools];
		if (TransformTools.hasKey(Key::CoordSystem))
		{
			CoordSystem = static_cast<EEditorCoordSystem>(TransformTools[Key::CoordSystem].ToInt());
		}
		if (TransformTools.hasKey(Key::bEnableTranslationSnap))
		{
			bEnableTranslationSnap = TransformTools[Key::bEnableTranslationSnap].ToBool();
		}
		if (TransformTools.hasKey(Key::TranslationSnapSize))
		{
			TranslationSnapSize = (std::max)(static_cast<float>(TransformTools[Key::TranslationSnapSize].ToFloat()), 0.00001f);
		}
		if (TransformTools.hasKey(Key::bEnableRotationSnap))
		{
			bEnableRotationSnap = TransformTools[Key::bEnableRotationSnap].ToBool();
		}
		if (TransformTools.hasKey(Key::RotationSnapSize))
		{
			RotationSnapSize = (std::max)(static_cast<float>(TransformTools[Key::RotationSnapSize].ToFloat()), 0.00001f);
		}
		if (TransformTools.hasKey(Key::bEnableScaleSnap))
		{
			bEnableScaleSnap = TransformTools[Key::bEnableScaleSnap].ToBool();
		}
		if (TransformTools.hasKey(Key::ScaleSnapSize))
		{
			ScaleSnapSize = (std::max)(static_cast<float>(TransformTools[Key::ScaleSnapSize].ToFloat()), 0.00001f);
		}
	}

	if (Root.hasKey(Key::RenderOptions))
	{
		JSON RenderOptionsObject = Root[Key::RenderOptions];
		LoadRenderOptions(RenderOptionsObject, RenderOptions);
	}
}

namespace
{
json::JSON MakeVectorJson(const FVector& Value)
{
	return json::Array(Value.X, Value.Y, Value.Z);
}

void LoadVector(json::JSON& Object, const char* KeyName, FVector& OutValue)
{
	if (!Object.hasKey(KeyName))
	{
		return;
	}

	json::JSON Value = Object[KeyName];
	OutValue = FVector(
		static_cast<float>(Value[0].ToFloat()),
		static_cast<float>(Value[1].ToFloat()),
		static_cast<float>(Value[2].ToFloat()));
}

void SaveRenderOptions(json::JSON& Object, const FViewportRenderOptions& Options)
{
	Object[Key::ViewMode] = static_cast<int32>(Options.ViewMode);
	Object[Key::ViewportType] = static_cast<int32>(Options.ViewportType);
	Object[Key::bPrimitives] = Options.ShowFlags.bPrimitives;
	Object[Key::bSkeletalMesh] = Options.ShowFlags.bSkeletalMesh;
	Object[Key::bGrid] = Options.ShowFlags.bGrid;
	Object[Key::bWorldAxis] = Options.ShowFlags.bWorldAxis;
	Object[Key::bGizmo] = Options.ShowFlags.bGizmo;
	Object[Key::bBillboardText] = Options.ShowFlags.bBillboardText;
	Object[Key::bBoundingVolume] = Options.ShowFlags.bBoundingVolume;
	Object[Key::bDebugDraw] = Options.ShowFlags.bDebugDraw;
	Object[Key::bSceneBVH] = Options.ShowFlags.bSceneBVH;
	Object[Key::bOctree] = Options.ShowFlags.bOctree;
	Object[Key::bWorldBound] = Options.ShowFlags.bWorldBound;
	Object[Key::bLightVisualization] = Options.ShowFlags.bLightVisualization;
	Object[Key::bLightHitMap] = Options.ShowFlags.bLightHitMap;
	Object[Key::bFog] = Options.ShowFlags.bFog;
	Object[Key::bFXAA] = Options.ShowFlags.bFXAA;
	Object[Key::bViewLightCulling] = Options.ShowFlags.bViewLightCulling;
	Object[Key::bVisualize25DCulling] = Options.ShowFlags.bVisualize25DCulling;
	Object[Key::bShowShadowFrustum] = Options.ShowFlags.bShowShadowFrustum;
	Object[Key::bGammaCorrection] = Options.ShowFlags.bGammaCorrection;
	Object[Key::bSelectionOutline] = Options.ShowFlags.bSelectionOutline;
	Object[Key::GridSpacing] = Options.GridSpacing;
	Object[Key::GridHalfLineCount] = Options.GridHalfLineCount;
	Object[Key::GridLineThickness] = Options.GridRenderSettings.LineThickness;
	Object[Key::GridMajorLineThickness] = Options.GridRenderSettings.MajorLineThickness;
	Object[Key::GridMajorLineInterval] = Options.GridRenderSettings.MajorLineInterval;
	Object[Key::GridMinorIntensity] = Options.GridRenderSettings.MinorIntensity;
	Object[Key::GridMajorIntensity] = Options.GridRenderSettings.MajorIntensity;
	Object[Key::GridAxisThickness] = Options.GridRenderSettings.AxisThickness;
	Object[Key::GridAxisIntensity] = Options.GridRenderSettings.AxisIntensity;
	Object[Key::CameraMoveSensitivity] = Options.CameraMoveSensitivity;
	Object[Key::CameraRotateSensitivity] = Options.CameraRotateSensitivity;
	Object[Key::DisplayGamma] = Options.DisplayGamma;
	Object[Key::GammaCorrectionBlend] = Options.GammaCorrectionBlend;
	Object[Key::bUseSRGBCurve] = Options.bUseSRGBCurve;
	Object[Key::DebugLineThickness] = Options.DebugLineThickness;
	Object[Key::ActorHelperBillboardScale] = Options.ActorHelperBillboardScale;
	Object[Key::DirectionalLightVisualizationScale] = Options.DirectionalLightVisualizationScale;
	Object[Key::PointLightVisualizationScale] = Options.PointLightVisualizationScale;
	Object[Key::SpotLightVisualizationScale] = Options.SpotLightVisualizationScale;
}

void LoadRenderOptions(json::JSON& Object, FViewportRenderOptions& Options)
{
	if (Object.hasKey(Key::ViewMode))
	{
		Options.ViewMode = static_cast<EViewMode>(Object[Key::ViewMode].ToInt());
	}
	if (Object.hasKey(Key::ViewportType))
	{
		Options.ViewportType = static_cast<ELevelViewportType>(Object[Key::ViewportType].ToInt());
	}
	if (Object.hasKey(Key::bPrimitives)) Options.ShowFlags.bPrimitives = Object[Key::bPrimitives].ToBool();
	if (Object.hasKey(Key::bSkeletalMesh)) Options.ShowFlags.bSkeletalMesh = Object[Key::bSkeletalMesh].ToBool();
	if (Object.hasKey(Key::bGrid)) Options.ShowFlags.bGrid = Object[Key::bGrid].ToBool();
	if (Object.hasKey(Key::bWorldAxis)) Options.ShowFlags.bWorldAxis = Object[Key::bWorldAxis].ToBool();
	if (Object.hasKey(Key::bGizmo)) Options.ShowFlags.bGizmo = Object[Key::bGizmo].ToBool();
	if (Object.hasKey(Key::bBillboardText)) Options.ShowFlags.bBillboardText = Object[Key::bBillboardText].ToBool();
	if (Object.hasKey(Key::bBoundingVolume)) Options.ShowFlags.bBoundingVolume = Object[Key::bBoundingVolume].ToBool();
	if (Object.hasKey(Key::bDebugDraw)) Options.ShowFlags.bDebugDraw = Object[Key::bDebugDraw].ToBool();
	if (Object.hasKey(Key::bSceneBVH)) Options.ShowFlags.bSceneBVH = Object[Key::bSceneBVH].ToBool();
	if (Object.hasKey(Key::bOctree)) Options.ShowFlags.bOctree = Object[Key::bOctree].ToBool();
	if (Object.hasKey(Key::bWorldBound)) Options.ShowFlags.bWorldBound = Object[Key::bWorldBound].ToBool();
	if (Object.hasKey(Key::bLightVisualization)) Options.ShowFlags.bLightVisualization = Object[Key::bLightVisualization].ToBool();
	if (Object.hasKey(Key::bLightHitMap)) Options.ShowFlags.bLightHitMap = Object[Key::bLightHitMap].ToBool();
	if (Object.hasKey(Key::bFog)) Options.ShowFlags.bFog = Object[Key::bFog].ToBool();
	if (Object.hasKey(Key::bFXAA)) Options.ShowFlags.bFXAA = Object[Key::bFXAA].ToBool();
	if (Object.hasKey(Key::bViewLightCulling)) Options.ShowFlags.bViewLightCulling = Object[Key::bViewLightCulling].ToBool();
	if (Object.hasKey(Key::bVisualize25DCulling)) Options.ShowFlags.bVisualize25DCulling = Object[Key::bVisualize25DCulling].ToBool();
	if (Object.hasKey(Key::bShowShadowFrustum)) Options.ShowFlags.bShowShadowFrustum = Object[Key::bShowShadowFrustum].ToBool();
	if (Object.hasKey(Key::bGammaCorrection)) Options.ShowFlags.bGammaCorrection = Object[Key::bGammaCorrection].ToBool();
	if (Object.hasKey(Key::bSelectionOutline)) Options.ShowFlags.bSelectionOutline = Object[Key::bSelectionOutline].ToBool();
	if (Object.hasKey(Key::GridSpacing)) Options.GridSpacing = std::clamp(static_cast<float>(Object[Key::GridSpacing].ToFloat()), 0.01f, 100.0f);
	if (Object.hasKey(Key::GridHalfLineCount)) Options.GridHalfLineCount = std::clamp<int32>(Object[Key::GridHalfLineCount].ToInt(), 1, 5000);
	if (Object.hasKey(Key::GridLineThickness)) Options.GridRenderSettings.LineThickness = std::clamp(static_cast<float>(Object[Key::GridLineThickness].ToFloat()), 0.0f, 8.0f);
	if (Object.hasKey(Key::GridMajorLineThickness)) Options.GridRenderSettings.MajorLineThickness = std::clamp(static_cast<float>(Object[Key::GridMajorLineThickness].ToFloat()), 0.0f, 12.0f);
	if (Object.hasKey(Key::GridMajorLineInterval)) Options.GridRenderSettings.MajorLineInterval = std::clamp<int32>(Object[Key::GridMajorLineInterval].ToInt(), 1, 100);
	if (Object.hasKey(Key::GridMinorIntensity)) Options.GridRenderSettings.MinorIntensity = std::clamp(static_cast<float>(Object[Key::GridMinorIntensity].ToFloat()), 0.0f, 2.0f);
	if (Object.hasKey(Key::GridMajorIntensity)) Options.GridRenderSettings.MajorIntensity = std::clamp(static_cast<float>(Object[Key::GridMajorIntensity].ToFloat()), 0.0f, 2.0f);
	if (Object.hasKey(Key::GridAxisThickness)) Options.GridRenderSettings.AxisThickness = std::clamp(static_cast<float>(Object[Key::GridAxisThickness].ToFloat()), 0.0f, 12.0f);
	if (Object.hasKey(Key::GridAxisIntensity)) Options.GridRenderSettings.AxisIntensity = std::clamp(static_cast<float>(Object[Key::GridAxisIntensity].ToFloat()), 0.0f, 2.0f);
	if (Object.hasKey(Key::CameraMoveSensitivity)) Options.CameraMoveSensitivity = static_cast<float>(Object[Key::CameraMoveSensitivity].ToFloat());
	if (Object.hasKey(Key::CameraRotateSensitivity)) Options.CameraRotateSensitivity = static_cast<float>(Object[Key::CameraRotateSensitivity].ToFloat());
	if (Object.hasKey(Key::DisplayGamma)) Options.DisplayGamma = static_cast<float>(Object[Key::DisplayGamma].ToFloat());
	if (Object.hasKey(Key::GammaCorrectionBlend)) Options.GammaCorrectionBlend = static_cast<float>(Object[Key::GammaCorrectionBlend].ToFloat());
	if (Object.hasKey(Key::bUseSRGBCurve)) Options.bUseSRGBCurve = Object[Key::bUseSRGBCurve].ToBool();
	if (Object.hasKey(Key::DebugLineThickness)) Options.DebugLineThickness = static_cast<float>(Object[Key::DebugLineThickness].ToFloat());
	if (Object.hasKey(Key::ActorHelperBillboardScale)) Options.ActorHelperBillboardScale = static_cast<float>(Object[Key::ActorHelperBillboardScale].ToFloat());
	if (Object.hasKey(Key::DirectionalLightVisualizationScale)) Options.DirectionalLightVisualizationScale = static_cast<float>(Object[Key::DirectionalLightVisualizationScale].ToFloat());
	if (Object.hasKey(Key::PointLightVisualizationScale)) Options.PointLightVisualizationScale = static_cast<float>(Object[Key::PointLightVisualizationScale].ToFloat());
	if (Object.hasKey(Key::SpotLightVisualizationScale)) Options.SpotLightVisualizationScale = static_cast<float>(Object[Key::SpotLightVisualizationScale].ToFloat());
}
}
