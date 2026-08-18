#include "Source/Engine/Public/GUI/Tabbar.h"
#include "ImGui/imgui.h"
#include "Source/Engine/Public/GUI/GuiMsg.h"

FTabbar::FTabbar()
{
    ToolbarElementSize = ImVec2(8, 10);
}

FGuiMsg FTabbar::ShowTabbar(ImVec2 InPos, ImVec2 InSize)
{
    ImGui::SetNextWindowPos(InPos);
    ImGui::SetNextWindowSize(InSize);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ToolbarElementSize);

    FGuiMsg resultMsg;
    if (ImGui::BeginTabBar("Tabbar"))
    {
        ImGuiIO& io = ImGui::GetIO();
        int ElementCount = Elements.size();

        for (int i = 0; i < ElementCount; i++)
        {
            FGuiWindow& Element = *Elements[i];
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
        ImGui::EndTabBar();
    }

    ImGui::PopStyleVar();
    return resultMsg;
}
