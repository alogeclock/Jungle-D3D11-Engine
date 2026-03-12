#include "pch.h"
#include "MinimapWidget.h"
#include "Gameplay/Stage.h"
#include "Render/Renderer.h"
#include "Render/Texture.h"
#include "Render/TextureManager.h"
#include "Render/FontManager.h"
#include <cmath>
#include <cfloat>

// 바늘 이미지가 1시 방향(15° CW)을 기본으로 가리키므로 보정
static constexpr float NeedleOffset = 3.14159265f / 12.0f;

void FMinimapWidget::BindStage(const FStage *InStage) { Stage = InStage; }

void FMinimapWidget::Update(FGameContext &Context)
{
    if (!Stage)
        return;

    int PlayerX = Stage->GetPlayer().GetTileX();
    int PlayerY = Stage->GetPlayer().GetTileY();

    // Exit compass angle
    int GoalX = Stage->GetGoalX();
    int GoalY = Stage->GetGoalY();
    if (GoalX >= 0 && GoalY >= 0)
    {
        float DX = static_cast<float>(GoalX - PlayerX);
        float DY = static_cast<float>(GoalY - PlayerY);
        // atan2: screen Y is down, tile Y is up → negate DY
        ExitAngle = atan2f(DX, -DY) - NeedleOffset;
    }

    // Nearest item compass angle
    bHasNearestItem = false;
    float       MinDist = FLT_MAX;
    int         NearestX = -1, NearestY = -1;
    const auto &Items = Stage->GetItems();
    for (const auto &Item : Items)
    {
        if (Item.bPickedUp)
            continue;
        float DX = static_cast<float>(Item.X - PlayerX);
        float DY = static_cast<float>(Item.Y - PlayerY);
        float Dist = DX * DX + DY * DY;
        if (Dist < MinDist)
        {
            MinDist = Dist;
            NearestX = Item.X;
            NearestY = Item.Y;
        }
    }

    if (NearestX >= 0 && NearestY >= 0)
    {
        bHasNearestItem = true;
        float DX = static_cast<float>(NearestX - PlayerX);
        float DY = static_cast<float>(NearestY - PlayerY);
        ItemAngle = atan2f(DX, -DY) - NeedleOffset;
    }
}

void FMinimapWidget::Render(FGameContext &Context)
{
    if (!Stage)
        return;

    float ScreenW = static_cast<float>(Context.Renderer.GetScreenWidth());
    float ScreenH = static_cast<float>(Context.Renderer.GetScreenHeight());

    const float CompassSize = 180.0f;
    const float NeedleSize = 140.0f;
    const float Padding = 16.0f;
    const float MarginRight = 30.0f;
    const float MarginBottom = 30.0f;
    const float LabelFontSize = 16.0f;
    const float LabelHeight = LabelFontSize + 6.0f;

    FTexture *CompassTex = Context.Textures.Get("compass");
    FTexture *NeedleTex = Context.Textures.Get("compass_needle");

    // Exit compass (right side)
    float ExitCX = ScreenW - MarginRight - CompassSize * 0.5f;
    float ExitCY = ScreenH - MarginBottom - CompassSize * 0.5f;

    if (CompassTex)
        Context.Renderer.DrawTexture(CompassTex, ExitCX, ExitCY, CompassSize, CompassSize);
    if (NeedleTex)
        Context.Renderer.DrawTexture(NeedleTex, ExitCX, ExitCY, NeedleSize, NeedleSize, ExitAngle);

    // Item compass (left of exit compass)
    // float ItemCX = ExitCX - CompassSize - Padding;
    // float ItemCY = ExitCY;

    // if (bHasNearestItem)
    //{
    //	if (CompassTex)
    //		Context.Renderer.DrawTexture(CompassTex, ItemCX, ItemCY, CompassSize, CompassSize);
    //	if (NeedleTex)
    //		Context.Renderer.DrawTexture(NeedleTex, ItemCX, ItemCY, NeedleSize, NeedleSize,
    // ItemAngle);
    // }

    //// Labels
    // if (FontTexPair* Pair = Context.FontManager.Get("basic_font"))
    //{
    //	float ExitLabelX = ExitCX - LabelFontSize * 2.0f * 0.25f;
    //	float ExitLabelY = ExitCY - CompassSize * 0.5f - LabelHeight;
    //	Context.Renderer.DrawFont("EXIT", Pair->Font.get(), Pair->Tex.get(),
    //		ExitLabelX, ExitLabelY, LabelFontSize);

    //	if (bHasNearestItem)
    //	{
    //		float ItemLabelX = ItemCX - LabelFontSize * 2.0f * 0.25f;
    //		float ItemLabelY = ItemCY - CompassSize * 0.5f - LabelHeight;
    //		Context.Renderer.DrawFont("ITEM", Pair->Font.get(), Pair->Tex.get(),
    //			ItemLabelX, ItemLabelY, LabelFontSize);
    //	}
    //}
}
