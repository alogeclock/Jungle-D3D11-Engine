#include "Source/Engine/Public/GUI/Toolbar.h"
#include "ImGui/imgui.h"
#include "Source/Engine/Public/GUI/GuiMsg.h"
#include "Source/Engine/Public/GUI/SceneWindow.h"


FToolbar::FToolbar()
{
    ToolbarElementSize = ImVec2(8, 10);

    Elements.push_back(new FSceneWindow("Scene"));
}

FGuiMsg FToolbar::ShowToolbar(ImVec2 InPos, ImVec2 InSize)
{
    ImGui::SetNextWindowPos(InPos);
    ImGui::SetNextWindowSize(InSize);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ToolbarElementSize);

    ImGui::Begin("##Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);

    ImGuiIO& io = ImGui::GetIO();
    int ElementCount = Elements.size();

    for (int i = 0; i < ElementCount; i++)
    {
        FGuiWindow &Element = *Elements[i];
        if (ImGui::SmallButton(Element.GetName().c_str()))
        {
            Element.SetVisible(!Element.GetVisable());
        }
        ImGui::SameLine();
    }

    float width = ToolbarElementSize.x;
    float height = ToolbarElementSize.y;
    FGuiMsg resultMsg;
    for (int i = 0; i < ElementCount; i++)
    {
        FGuiWindow& Element = *Elements[i];
        if (Element.GetVisable())
        {
            resultMsg = Element.ShowPanel({width * i, InSize.y});
            if (resultMsg.bIsExcetion)
                break;
        }
            

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse)
        {
            Element.SetVisible(false);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    return resultMsg;
}
