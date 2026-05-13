#include "ContentBrowserElement.h"
#include "Editor/UI/EditorFileUtils.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/UI/AssetEditor/AssetEditorWidget.h"
#include "Platform/Paths.h"
#include "Core/Notification.h"
#include "Mesh/FbxImporter.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshManager.h"
#include "Engine/Runtime/Engine.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <vector>

namespace
{
	std::wstring ToLowerWide(std::wstring Text)
	{
		std::transform(Text.begin(), Text.end(), Text.begin(), ::towlower);
		return Text;
	}

	bool IsMatchingSkeletalMeshCache(const std::filesystem::path& CandidatePath, const std::wstring& ExpectedStemLower)
	{
		if (ToLowerWide(CandidatePath.extension().wstring()) != L".skm")
		{
			return false;
		}

		return ToLowerWide(CandidatePath.stem().wstring()) == ExpectedStemLower;
	}

	std::filesystem::path FindNearbySkeletalMeshCache(const std::filesystem::path& FbxPath)
	{
		const std::filesystem::path SearchRoot = FbxPath.parent_path();
		if (SearchRoot.empty())
		{
			return {};
		}

		const std::wstring ExpectedStemLower = ToLowerWide(FbxPath.stem().wstring());
		const std::filesystem::path DirectCachePath = SearchRoot / L"Cache" / (FbxPath.stem().wstring() + L".skm");
		const std::filesystem::path DirectPath = SearchRoot / (FbxPath.stem().wstring() + L".skm");

		std::error_code ErrorCode;
		if (std::filesystem::is_regular_file(DirectCachePath, ErrorCode))
		{
			return DirectCachePath;
		}

		ErrorCode.clear();
		if (std::filesystem::is_regular_file(DirectPath, ErrorCode))
		{
			return DirectPath;
		}

		const std::filesystem::path CacheRoot = SearchRoot / L"Cache";
		std::filesystem::directory_iterator It(CacheRoot, std::filesystem::directory_options::skip_permission_denied, ErrorCode);
		const std::filesystem::directory_iterator End;
		while (!ErrorCode && It != End)
		{
			const std::filesystem::path CandidatePath = It->path();
			if (std::filesystem::is_regular_file(CandidatePath, ErrorCode) && IsMatchingSkeletalMeshCache(CandidatePath, ExpectedStemLower))
			{
				return CandidatePath;
			}

			ErrorCode.clear();
			It.increment(ErrorCode);
		}

		return {};
	}

	bool IsFreshCacheFile(const std::filesystem::path& CachePath, const std::filesystem::path& SourcePath)
	{
		std::error_code ErrorCode;
		if (!std::filesystem::is_regular_file(CachePath, ErrorCode))
		{
			return false;
		}

		ErrorCode.clear();
		if (!std::filesystem::is_regular_file(SourcePath, ErrorCode))
		{
			return true;
		}

		ErrorCode.clear();
		const auto CacheWriteTime = std::filesystem::last_write_time(CachePath, ErrorCode);
		if (ErrorCode)
		{
			return false;
		}

		ErrorCode.clear();
		const auto SourceWriteTime = std::filesystem::last_write_time(SourcePath, ErrorCode);
		return !ErrorCode && CacheWriteTime >= SourceWriteTime;
	}

	USkeletalMesh* TryLoadNearbySkeletalMeshCache(const std::filesystem::path& FbxPath, FString& OutRelativeSkmPath)
	{
		const std::filesystem::path CachePath = FindNearbySkeletalMeshCache(FbxPath);
		if (CachePath.empty())
		{
			return nullptr;
		}

		OutRelativeSkmPath = FPaths::ToUtf8(CachePath.lexically_relative(FPaths::RootDir()).generic_wstring());
		return FSkeletalMeshManager::LoadSkeletalMesh(OutRelativeSkmPath);
	}

	UStaticMesh* TryLoadFreshStaticMeshCache(const FString& RelativeFbxPath, const std::filesystem::path& FbxPath, ID3D11Device* Device, FString& OutRelativeBinPath)
	{
		if (!Device)
		{
			return nullptr;
		}

		const FString BinPath = FObjManager::GetBinaryFilePath(RelativeFbxPath);
		const std::filesystem::path BinPathW(FPaths::ToWide(FPaths::ConvertRelativePathToFull(BinPath)));
		if (!IsFreshCacheFile(BinPathW, FbxPath))
		{
			return nullptr;
		}

		OutRelativeBinPath = BinPath;
		return FObjManager::LoadFbxStaticMesh(RelativeFbxPath, Device);
	}

	std::vector<FString> WrapTextLines(const FString& Text, float MaxWidth, int MaxLines)
	{
		std::vector<FString> Lines;
		if (Text.empty() || MaxLines <= 0)
		{
			return Lines;
		}

		FString CurrentLine;
		for (char Character : Text)
		{
			FString Candidate = CurrentLine;
			Candidate.push_back(Character);

			if (!CurrentLine.empty() && ImGui::CalcTextSize(Candidate.c_str()).x > MaxWidth)
			{
				Lines.push_back(CurrentLine);
				CurrentLine.clear();
				if (static_cast<int>(Lines.size()) >= MaxLines)
				{
					break;
				}
			}

			if (static_cast<int>(Lines.size()) >= MaxLines)
			{
				break;
			}

			CurrentLine.push_back(Character);
		}

		if (static_cast<int>(Lines.size()) < MaxLines && !CurrentLine.empty())
		{
			Lines.push_back(CurrentLine);
		}

		if (Lines.empty())
		{
			Lines.push_back(Text);
		}

		if (static_cast<int>(Lines.size()) > MaxLines)
		{
			Lines.resize(MaxLines);
		}

		size_t UsedCharacters = 0;
		for (const FString& Line : Lines)
		{
			UsedCharacters += Line.size();
		}

		FString& LastLine = Lines.back();
		while (!LastLine.empty() && ImGui::CalcTextSize((LastLine + "...").c_str()).x > MaxWidth)
		{
			LastLine.pop_back();
		}

		if (UsedCharacters < Text.size())
		{
			LastLine += "...";
		}

		return Lines;
	}

	std::filesystem::path GetProtectedContentRoot()
	{
		return (std::filesystem::path(FPaths::RootDir()) / L"Asset" / L"Content").lexically_normal();
	}

	std::filesystem::path GetProtectedScriptsRoot()
	{
		return (std::filesystem::path(FPaths::RootDir()).parent_path() / L"Scripts").lexically_normal();
	}

	bool IsProtectedContentBrowserPath(const std::filesystem::path& InPath)
	{
		const std::filesystem::path NormalizedPath = InPath.lexically_normal();
		return NormalizedPath == GetProtectedContentRoot() || NormalizedPath == GetProtectedScriptsRoot();
	}
}

bool ContentBrowserElement::RenderSelectSpace(ContentBrowserContext& Context)
{
	FString Name = FPaths::ToUtf8(ContentItem.Name);
	ImGui::PushID(Name.c_str());

	bIsSelected = Context.SelectedElement.get() == this;

	ImGui::InvisibleButton("##Element", Context.ContentSize);
	bool bIsClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

	ImVec2 Min = ImGui::GetItemRectMin();
	ImVec2 Max = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	ImFont* font = ImGui::GetFont();
	const float fontSize = ImGui::GetFontSize();
	const float Padding = 8.0f;
	const float TextSpacing = 5.0f;
	const float WrapWidth = (Max.x - Min.x) - Padding * 2.0f;
	const FString DisplayName = GetDisplayName();
	const FString Subtitle = GetSubtitleText();
	const ImVec2 SubtitleTextSize = ImGui::CalcTextSize(Subtitle.c_str());
	const float CardRounding = 6.0f;
	const ImU32 BorderColor = bIsSelected ? EditorAccentColor::ToU32() : IM_COL32(62, 62, 66, 255);
	const ImU32 FillColor = bIsSelected ? EditorAccentColor::ToU32(48) : IM_COL32(36, 36, 39, 255);
	const ImU32 ThumbnailFill = IM_COL32(30, 30, 33, 255);
	DrawList->AddRectFilled(Min, Max, FillColor, CardRounding);
	DrawList->AddRect(Min, Max, BorderColor, CardRounding, 0, bIsSelected ? 1.5f : 1.0f);

	const float ThumbnailHeight = (std::min)(54.0f, (Max.y - Min.y) * 0.48f);
	ImVec2 ThumbnailMin(Min.x + Padding, Min.y + Padding);
	ImVec2 ThumbnailMax(Max.x - Padding, ThumbnailMin.y + ThumbnailHeight);
	DrawList->AddRectFilled(ThumbnailMin, ThumbnailMax, ThumbnailFill, 5.0f);
	DrawList->AddRect(ThumbnailMin, ThumbnailMax, IM_COL32(52, 52, 56, 255), 5.0f);

	const float IconSize = IsTexturePreview() ? ThumbnailHeight - 2.0f : (std::min)(34.0f, ThumbnailHeight - 12.0f);
	ImVec2 ImageMin(
		ThumbnailMin.x + ((ThumbnailMax.x - ThumbnailMin.x) - IconSize) * 0.5f,
		ThumbnailMin.y + ((ThumbnailMax.y - ThumbnailMin.y) - IconSize) * 0.5f);
	ImVec2 ImageMax(ImageMin.x + IconSize, ImageMin.y + IconSize);
	if (Icon)
	{
		DrawList->AddImage(Icon, ImageMin, ImageMax, ImVec2(0, 0), ImVec2(1, 1), GetIconTint());
	}

	const float NameTop = ThumbnailMax.y + TextSpacing;
	const auto Lines = WrapTextLines(DisplayName, WrapWidth, 2);
	for (int LineIndex = 0; LineIndex < static_cast<int>(Lines.size()); ++LineIndex)
	{
		const ImVec2 TextSize = ImGui::CalcTextSize(Lines[LineIndex].c_str());
		const float TextX = Min.x + ((Max.x - Min.x) - TextSize.x) * 0.5f;
		const float TextY = NameTop + LineIndex * (fontSize + 2.0f);
		DrawList->AddText(font, fontSize, ImVec2(TextX, TextY), ImGui::GetColorU32(ImGuiCol_Text), Lines[LineIndex].c_str());
	}

	if (!Subtitle.empty())
	{
		const float SubtitleX = Min.x + ((Max.x - Min.x) - SubtitleTextSize.x) * 0.5f;
		const float SubtitleY = Max.y - Padding - SubtitleTextSize.y;
		DrawList->AddText(ImVec2(SubtitleX, SubtitleY), IM_COL32(155, 155, 160, 255), Subtitle.c_str());
	}
	ImGui::PopID();

	return bIsClicked;
}

void ContentBrowserElement::Render(ContentBrowserContext& Context)
{
	if (RenderSelectSpace(Context))
	{
		Context.SelectedElement = shared_from_this();
		bIsSelected = true;
		OnLeftClicked(Context);
	}

	bool bDoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
	if (bDoubleClicked)
	{
		OnDoubleLeftClicked(Context);
	}

	if (ImGui::BeginPopupContextItem())
	{
		DrawContextMenu(Context);
		ImGui::EndPopup();
	}

	if (ImGui::BeginDragDropSource())
	{
		RenderSelectSpace(Context);
		ImGui::SetDragDropPayload(GetDragItemType(), &ContentItem, sizeof(ContentItem));
		OnDrag(Context);
		ImGui::EndDragDropSource();
	}
}

FString ContentBrowserElement::GetDisplayName() const
{
	if (ContentItem.bIsDirectory)
	{
		return FPaths::ToUtf8(ContentItem.Name);
	}

	return FPaths::ToUtf8(ContentItem.Path.stem().wstring());
}

FString ContentBrowserElement::GetSubtitleText() const
{
	if (ContentItem.bIsDirectory)
	{
		return "Folder";
	}

	FString Extension = FPaths::ToUtf8(ContentItem.Path.extension().wstring());
	if (!Extension.empty() && Extension.front() == '.')
	{
		Extension.erase(Extension.begin());
	}

	return Extension;
}

bool ContentBrowserElement::CanDelete() const
{
	return !IsProtectedContentBrowserPath(ContentItem.Path);
}

void ContentBrowserElement::DrawContextMenu(ContentBrowserContext& Context)
{
	const bool bCanOpen = std::filesystem::exists(ContentItem.Path);
	if (ImGui::MenuItem("Open", nullptr, false, bCanOpen))
	{
		OnDoubleLeftClicked(Context);
	}

	if (ImGui::MenuItem("Show in Explorer", nullptr, false, bCanOpen))
	{
		if (!FEditorFileUtils::RevealInExplorer(ContentItem.Path))
		{
			FNotificationManager::Get().AddNotification("Failed to reveal path in Explorer.", ENotificationType::Error, 5.0f);
		}
	}

	if (ImGui::MenuItem("Delete", nullptr, false, bCanOpen && CanDelete()))
	{
		if (FEditorFileUtils::DeletePath(ContentItem.Path))
		{
			if (Context.SelectedElement.get() == this)
			{
				Context.SelectedElement.reset();
			}
			Context.bIsNeedRefresh = true;
			FNotificationManager::Get().AddNotification("Deleted: " + GetDisplayName(), ENotificationType::Success, 3.0f);
		}
		else
		{
			FNotificationManager::Get().AddNotification("Failed to delete: " + GetDisplayName(), ENotificationType::Error, 5.0f);
		}
	}
}

void ContentBrowserElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	(void)Context;
	if (!FEditorFileUtils::OpenPath(ContentItem.Path))
	{
		FNotificationManager::Get().AddNotification("Failed to open: " + GetDisplayName(), ENotificationType::Error, 5.0f);
	}
}

void DirectoryElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	Context.CurrentPath = ContentItem.Path;
	Context.PendingRevealPath = ContentItem.Path;
	Context.bIsNeedRefresh = true;
}

#include "Serialization/SceneSaveManager.h"
#include "Editor/EditorEngine.h"
void SceneElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	std::filesystem::path ScenePath = ContentItem.Path;
	FString FilePath = FPaths::ToUtf8(ScenePath.wstring());
	UEditorEngine* EditorEngine = Context.EditorEngine;
	EditorEngine->LoadSceneFromPath(FilePath);
}

void MaterialElement::OnLeftClicked(ContentBrowserContext& Context)
{
	MaterialInspector = { ContentItem.Path };
}

void MaterialElement::RenderDetail()
{
	MaterialInspector.Render();
}

void MtlElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	const FString RelativeMtlPath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());
	FNotificationManager::Get().AddNotification("MTL import is no longer supported directly.", ENotificationType::Error, 4.0f);
}


void UAssetElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	(void)Context;
	if (!FAssetEditorWidget::OpenAssetFile(ContentItem.Path))
	{
		FNotificationManager::Get().AddNotification("Failed to open asset: " + GetDisplayName(), ENotificationType::Error, 5.0f);
	}
}

void FbxElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	const FString RelativeFbxPath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());

	FString RelativeSkmPath;
	if (USkeletalMesh* CachedSkeletalMesh = TryLoadNearbySkeletalMeshCache(ContentItem.Path, RelativeSkmPath))
	{
		Context.bIsNeedRefresh = true;

		if (Context.EditorEngine)
		{
			Context.EditorEngine->OpenSkeletalMeshEditor(CachedSkeletalMesh);
		}

		FNotificationManager::Get().AddNotification("Loaded SkeletalMesh cache: " + RelativeSkmPath, ENotificationType::Success, 4.0f);
		return;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;

	FString RelativeBinPath;
	if (UStaticMesh* CachedStaticMesh = TryLoadFreshStaticMeshCache(RelativeFbxPath, ContentItem.Path, Device, RelativeBinPath))
	{
		(void)CachedStaticMesh;
		Context.bIsNeedRefresh = true;
		FNotificationManager::Get().AddNotification("Loaded StaticMesh cache: " + RelativeBinPath, ENotificationType::Success, 4.0f);
		return;
	}

	if (FFbxImporter::HasSkinDeformer(RelativeFbxPath))
	{
		USkeletalMesh* SkeletalMesh = FSkeletalMeshManager::LoadSkeletalMesh(RelativeFbxPath);

		if (!SkeletalMesh)
		{
			FNotificationManager::Get().AddNotification("Failed to open SkeletalMesh: " + RelativeFbxPath, ENotificationType::Error, 6.0f);
			return;
		}

		Context.bIsNeedRefresh = true;

		if (Context.EditorEngine)
		{
			Context.EditorEngine->OpenSkeletalMeshEditor(SkeletalMesh);
		}

		FNotificationManager::Get().AddNotification("Loaded SkeletalMesh: " + RelativeFbxPath, ENotificationType::Success, 4.0f);

		return;
	}

	if (!Device)
	{
		FNotificationManager::Get().AddNotification("Failed to open StaticMesh FBX: D3D device is null.", ENotificationType::Error, 6.0f);
		return;
	}

	UStaticMesh* StaticMesh = FObjManager::LoadFbxStaticMesh(RelativeFbxPath, Device);

	if (!StaticMesh)
	{
		FNotificationManager::Get().AddNotification("Failed to open StaticMesh: " + RelativeFbxPath, ENotificationType::Error, 6.0f);
		return;
	}

	Context.bIsNeedRefresh = true;
	FNotificationManager::Get().AddNotification("Loaded StaticMesh: " + RelativeFbxPath, ENotificationType::Success, 4.0f);
}

void SkeletalMeshElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	const FString RelativeSkmPath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());

	USkeletalMesh* SkeletalMesh = FSkeletalMeshManager::LoadSkeletalMesh(RelativeSkmPath);

	if (!SkeletalMesh)
	{
		FNotificationManager::Get().AddNotification("Failed to open SkeletalMesh: " + RelativeSkmPath, ENotificationType::Error, 6.0f);
		return;
	}

	if (Context.EditorEngine)
	{
		Context.EditorEngine->OpenSkeletalMeshEditor(SkeletalMesh);
	}

	FNotificationManager::Get().AddNotification("Loaded SkeletalMesh: " + RelativeSkmPath, ENotificationType::Success, 4.0f);
}
