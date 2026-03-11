#include "pch.h"
#include "BeatHUDWidget.h"
#include "Core/Time.h"
#include "Render/Renderer.h"
#include "Render/TextureManager.h"
#include <cmath>

void FBeatHUDWidget::BindBeatSystem(const FBeatSystem* InBeatSystem)
{
	BeatSystem = InBeatSystem;
	Ypos = 800.f;
	Heart.X = 500.f;
	Heart.Y = Ypos;

}

void FBeatHUDWidget::Update(FGameContext& Context)
{
	if (!BarTexture)
	{
		BarTexture = Context.Textures.Get("beat_bar");
	}
	if (FlashTimer > 0.0f)
	{
		FlashTimer -= Context.Time.GetDeltaTime();
	}
}

void FBeatHUDWidget::Render(FGameContext& Context)
{

	if (!BeatSystem) return;

	float BeatInterval = BeatSystem->GetBeatInterval();
	//이전박자 ----- 현재 ----- 이후박자
	//    ElapsedTime   RemainedTime
	float RemainedTime = BeatSystem->GetTimeToNextBeat(); //거리계산에 사용
	float ElapsedTime = BeatInterval - RemainedTime;
	float DistanceRatio = RemainedTime / BeatInterval;
	float Distance = MaxDistance * DistanceRatio;

	float LeftBarX = Heart.X - Distance;
	float RightBarX = Heart.X + Distance;
	Context.Renderer.DrawTexture(BarTexture, LeftBarX, Ypos, 20, 100);
	Context.Renderer.DrawTexture(BarTexture, RightBarX, Ypos, 20, 100);
	/*if (!BeatSystem)
		return;

	float BeatInterval = BeatSystem->GetBeatInterval();
	float TimeToNext = BeatSystem->GetTimeToNextBeat();
	float TimeSinceLast = BeatInterval - TimeToNext;

	// 박자 기준 -0.5 ~ +0.5 범위로 정규화 (0 = 정박)
	// TimeSinceLast < TimeToNext: 지난 박자 이후 → 양수
	// TimeSinceLast > TimeToNext: 다음 박자 이전 → 음수
	float Normalized;
	if (TimeSinceLast <= TimeToNext)
	{
		Normalized = TimeSinceLast / BeatInterval; // 0 ~ 0.5
	}
	else
	{
		Normalized = -(TimeToNext / BeatInterval); // -0.5 ~ 0
	}

	// Good 판정 구간 비율
	float GoodRatio = BeatSystem->GetGoodWindow() / BeatInterval;

	// 윈도우 설정
	float ScreenW = Context.Renderer.GetScreenWidth();
	float SliderWidth = 400.0f;
	ImGui::SetNextWindowPos(ImVec2((ScreenW - SliderWidth - 30.0f) * 0.5f, 10.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(SliderWidth + 30.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.4f);

	ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoInputs
		| ImGuiWindowFlags_AlwaysAutoResize
		| ImGuiWindowFlags_NoFocusOnAppearing
		| ImGuiWindowFlags_NoNav;

	if (ImGui::Begin("##BeatHUD", nullptr, Flags))
	{
		ImDrawList *DrawList = ImGui::GetWindowDrawList();
		ImVec2 Pos = ImGui::GetCursorScreenPos();

		float BarHeight = 20.0f;
		float BarLeft = Pos.x;
		float BarRight = Pos.x + SliderWidth;
		float BarTop = Pos.y;
		float BarBottom = Pos.y + BarHeight;
		float BarCenter = (BarLeft + BarRight) * 0.5f;

		// 배경 바
		DrawList->AddRectFilled(
			ImVec2(BarLeft, BarTop),
			ImVec2(BarRight, BarBottom),
			IM_COL32(40, 40, 40, 200),
			4.0f);

		// Good 판정 허용 구간 (중앙 기준 좌우)
		float GoodHalf = GoodRatio * SliderWidth;
		DrawList->AddRectFilled(
			ImVec2(BarCenter - GoodHalf, BarTop),
			ImVec2(BarCenter + GoodHalf, BarBottom),
			IM_COL32(80, 200, 80, 100),
			4.0f);

		// 정박 중앙선
		DrawList->AddLine(
			ImVec2(BarCenter, BarTop),
			ImVec2(BarCenter, BarBottom),
			IM_COL32(255, 255, 255, 180),
			2.0f);

		// 현재 위치 커서 (Normalized: -0.5 ~ +0.5 → 바 범위)
		float CursorX = BarCenter + Normalized * SliderWidth;
		CursorX = fmaxf(BarLeft, fminf(CursorX, BarRight));

		// 판정에 따라 커서 색상 결정
		float Distance = (TimeToNext < TimeSinceLast) ? TimeToNext : TimeSinceLast;
		bool bIsInGood = (Distance <= BeatSystem->GetGoodWindow());
		ImU32 CursorColor = bIsInGood ? IM_COL32(100, 255, 100, 255) : IM_COL32(255, 80, 80, 255);

		DrawList->AddRectFilled(
			ImVec2(CursorX - 3.0f, BarTop - 2.0f),
			ImVec2(CursorX + 3.0f, BarBottom + 2.0f),
			CursorColor,
			2.0f);

		// 커서 영역 확보
		ImGui::Dummy(ImVec2(SliderWidth, BarHeight + 4.0f));
	}
	ImGui::End();*/
}
