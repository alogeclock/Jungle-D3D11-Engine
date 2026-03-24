#include "Source/Engine/Public/GUI/EngineStaticsGUI.h"
#include "Source/Editor/Public/Application.h"


void FEngineStaticsGUI::ShowPanel()
{
    ImGui::TextWrapped("FPS: %.f \t FrameTime: %.1f (ms)\n", UTimeManager::Get().GetFPS(),
                       UTimeManager::Get().GetFrameTime());
    ImGui::TextWrapped("Allocated Bytes: %u", TotalAllocationBytes);
    ImGui::TextWrapped("Allocated Count: %u", TotalAllocationCount);
    ImGui::TextWrapped("Object Count: %u", GUObjectArray.size());
}
