#include "GameViewWindow.h"
#include "EditorRegistry.h"
#include "RenderManager.h"

using namespace MMMEngine::EditorRegistry;
using namespace MMMEngine::Editor;
using namespace MMMEngine;

// GameViewWindow.cpp
void MMMEngine::Editor::GameViewWindow::Render()
{
    if (!g_editor_window_gameView)
        return;

    ImGuiWindowClass wc;
    wc.ParentViewportId = ImGui::GetMainViewport()->ID;
    wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
    ImGui::SetNextWindowClass(&wc);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_None;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin(u8"\uf11b 게임", &g_editor_window_gameView))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // ---- 여기부터는 "절대 return 하지 말고" 조건부로만 처리 ----
    UINT resX = 0, resY = 0;
    RenderManager::Get().GetSceneSize(resX, resY);

    ImVec2 contentRegion = ImGui::GetContentRegionAvail();

    if (resX > 0 && resY > 0 && contentRegion.x > 0.0f && contentRegion.y > 0.0f)
    {
        float targetAspect = (float)resX / (float)resY;

        ImVec2 displaySize;
        float windowAspect = contentRegion.x / contentRegion.y;

        if (windowAspect > targetAspect) {
            displaySize.y = contentRegion.y;
            displaySize.x = contentRegion.y * targetAspect;
        }
        else {
            displaySize.x = contentRegion.x;
            displaySize.y = contentRegion.x / targetAspect;
        }

        float offsetX = (contentRegion.x - displaySize.x) * 0.5f;
        float offsetY = (contentRegion.y - displaySize.y) * 0.5f;
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY));

        auto sceneSRV = RenderManager::Get().GetSceneSRV();
        if (sceneSRV.Get())
        {
            ImGui::Image((ImTextureID)sceneSRV.Get(), displaySize);
        }
        else
        {
            ImGui::TextUnformatted("Scene SRV is null.");
        }
    }
    else
    {
        ImGui::TextUnformatted("Invalid scene size or content region.");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}