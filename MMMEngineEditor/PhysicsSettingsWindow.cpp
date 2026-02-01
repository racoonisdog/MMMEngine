#include "PhysicsSettingsWindow.h"
#include "EditorRegistry.h"
#include "ProjectManager.h"
#include "PhysicsSettings.h"

#include <map>
#include "imgui.h"
#include "imgui_internal.h" // 회전 및 세부 제어를 위해 필요

using namespace MMMEngine;
using namespace MMMEngine::EditorRegistry;

void TextAligned(const char* label, float size_x, float align_x = 1.0f) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(size_x, ImGui::GetTextLineHeight());
    ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, 0)) return;

    // align_x: 0.0f(좌), 0.5f(중앙), 1.0f(우)
    ImGui::RenderTextClipped(bb.Min, bb.Max, label, NULL, NULL, ImVec2(align_x, 0.5f));
}

// 텍스트를 중심점 기준으로 회전시켜 그리는 헬퍼
void DrawRotatedText(const char* text, float angle) {
    ImGuiContext& g = *GImGui;
    ImVec2 size = ImGui::CalcTextSize(text);
    ImVec2 pos = ImGui::GetCursorScreenPos();

    // 회전 중심점 계산 (텍스트의 왼쪽 하단 기준)
    ImVec2 center = pos;

    // ImGui의 로우레벨 회전 API (imgui_internal.h)
    int vtx_idx_2 = ImGui::GetWindowDrawList()->VtxBuffer.Size;
    ImGui::RenderText(pos, text); // 먼저 텍스트를 그림
    int vtx_idx_3 = ImGui::GetWindowDrawList()->VtxBuffer.Size;

    // 그려진 텍스트 정점들만 골라서 회전 변환 적용
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    for (int n = vtx_idx_2; n < vtx_idx_3; n++) {
        ImDrawVert& v = ImGui::GetWindowDrawList()->VtxBuffer[n];
        ImVec2 rel = ImVec2(v.pos.x - center.x, v.pos.y - center.y);
        v.pos.x = center.x + rel.x * cos_a - rel.y * sin_a;
        v.pos.y = center.y + rel.x * sin_a + rel.y * cos_a;
    }
}

void ShowCollisionMatrix(const char** layerNames) {
    const float cellSize = 20.0f;
    const float labelWidth = 100.0f;

    auto& physXSet = PhysicsSettings::Get();

    // 가로 행 루프
    for (int i = 0; i < 32; i++) {
        TextAligned(layerNames[i], labelWidth, 1.0f); // 우측 정렬 이름
        ImGui::SameLine();

        for (int j = 0; j <= i; j++) {
            ImGui::PushID(i * 32 + j);

            // 비트 연산으로 체크박스 상태 관리
            bool isConnected = (physXSet.collisionMatrix[i] & (1 << j));
            if (ImGui::Checkbox("##cell", &isConnected)) {
                if (isConnected) {
                    physXSet.collisionMatrix[i] |= (1 << j);
                    physXSet.collisionMatrix[j] |= (1 << i); // 대칭 적용
                }
                else {
                    physXSet.collisionMatrix[i] &= ~(1 << j);
                    physXSet.collisionMatrix[j] &= ~(1 << i);
                }
            }

            if (j < i) ImGui::SameLine();
            ImGui::PopID();
        }
    }
}
void MMMEngine::Editor::PhysicsSettingsWindow::Render()
{
    if (!g_editor_window_physicsSettings)
        return;

    // 프로젝트가 활성화되어 있지 않으면 조작 불가
    if (!ProjectManager::Get().HasActiveProject()) return;

    // 싱글톤 참조
    auto& settings = PhysicsSettings::Get();
    bool wasOpen = g_editor_window_physicsSettings;

    ImGuiWindowClass wc;
    wc.ParentViewportId = ImGui::GetMainViewport()->ID;
    wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
    ImGui::SetNextWindowClass(&wc);

    if (ImGui::Begin("Physics Settings", &g_editor_window_physicsSettings, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
    {
        const float cellSize = 30.0f;
        const float labelWidth = 100.0f;
        const int count = 8; // 32까지 가능하지만 UI상 8로 고정하신 듯 함

        ImVec2 startPosRel = ImGui::GetCursorPos();

        // 1. 상단 세로 레이블
        for (int i = 0; i < count; i++) {
            int layerIdx = (count - 1) - i;
            float posX = startPosRel.x + labelWidth + (i * cellSize) + (cellSize * 0.5f) - 5.0f;
            float posY = startPosRel.y + labelWidth - 10.0f;

            ImGui::SetCursorPos(ImVec2(posX, posY));

            std::string displayName = std::to_string(layerIdx);
            if (settings.idToName.count(layerIdx)) {
                displayName = displayName + ": " + settings.idToName[layerIdx];
            }
            DrawRotatedText(displayName.c_str(), -IM_PI * 0.5f);
        }

        // 2. 매트릭스 본체 (settings.collisionMatrix 직접 수정)
        for (int i = 0; i < count; i++) {
            ImGui::SetCursorPos(ImVec2(startPosRel.x, startPosRel.y + labelWidth + (i * cellSize)));

            std::string rowName = std::to_string(i);
            if (settings.idToName.count(i)) {
                rowName = settings.idToName[i] + " (" + rowName + ")";
            }
            TextAligned(rowName.c_str(), labelWidth, 1.0f);

            for (int j = 0; j < (count - i); j++) {
                ImGui::PushID(i * 100 + j);
                int colIdx = (count - 1) - j;

                float cellPosX = startPosRel.x + labelWidth + (j * cellSize) + (cellSize * 0.15f);
                ImGui::SameLine();
                ImGui::SetCursorPosX(cellPosX);

                // 싱글톤 데이터 비트 연산
                bool isConnected = (settings.collisionMatrix[i] & (1 << colIdx));
                if (ImGui::Checkbox("##cell", &isConnected)) {
                    if (isConnected) {
                        settings.collisionMatrix[i] |= (1 << colIdx);
                        settings.collisionMatrix[colIdx] |= (1 << i);
                    }
                    else {
                        settings.collisionMatrix[i] &= ~(1 << colIdx);
                        settings.collisionMatrix[colIdx] &= ~(1 << i);
                    }
                }
                ImGui::PopID();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Layer Name Mapping");

        if (ImGui::BeginChild("LayerMap", ImVec2(0, 200), true)) {
            for (int i = 0; i < count; i++) {
                char buf[64];
                std::string currentName = settings.idToName.count(i) ? settings.idToName[i] : "";
                strcpy_s(buf, currentName.c_str());

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Layer %d:", i); ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20);
                if (ImGui::InputText(("##name" + std::to_string(i)).c_str(), buf, 64)) {
                    if (buf[0] == '\0') settings.idToName.erase(i);
                    else settings.idToName[i] = buf;
                }
            }
        }
        ImGui::EndChild();

        // 수동 적용 버튼 (선택 사항)
        if (ImGui::Button("Apply & Save Now")) {
            auto projectDir = ProjectManager::Get().GetActiveProject().ProjectRootFS();
            settings.ApplySettings();
            settings.SaveSettings();
        }
    }
    ImGui::End();

    // [중요] 창이 닫히는 순간 자동 저장 및 적용
    if (wasOpen && !g_editor_window_physicsSettings) {
        auto projectDir = ProjectManager::Get().GetActiveProject().ProjectRootFS();
        settings.ApplySettings(); // PhysX 매니저에 비트 전달
        settings.SaveSettings(); // MessagePack 저장
    }
}
