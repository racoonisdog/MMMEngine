#define NOMINMAX
#include "ImGuiEditorContext.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include "SceneManager.h"
#include "SceneSerializer.h"

#include "EditorRegistry.h"
#include "StringHelper.h"

using namespace MMMEngine::EditorRegistry;
using namespace MMMEngine::Utility;

#include "StartUpProjectWindow.h"
#include "SceneListWindow.h"
#include "HierarchyWindow.h"
#include "InspectorWindow.h"
#include "ScriptBuildWindow.h"
#include "ConsoleWindow.h"
#include "FilesWindow.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void MMMEngine::Editor::ImGuiEditorContext::UpdateInfiniteDrag()
{
    ImGuiIO& io = ImGui::GetIO();

    static ImVec2 lastMousePosBeforeWarp = ImVec2(-1, 1);

    // 드래그 중이 아니면 리셋
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        lastMousePosBeforeWarp = ImVec2(-1, -1);
        return;
    }

    // 현재 윈도우 핸들 가져오기
    HWND hwnd = GetActiveWindow(); // 또는 메인 윈도우 핸들 저장해두기
    if (!hwnd) return;

    // 현재 마우스 위치 (스크린 좌표)
    POINT screenPos;
    GetCursorPos(&screenPos);

    // 윈도우 클라이언트 영역 정보
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    POINT clientOrigin = { 0, 0 };
    ClientToScreen(hwnd, &clientOrigin);

    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    // 클라이언트 영역 기준으로 변환
    int localX = screenPos.x - clientOrigin.x;
    int localY = screenPos.y - clientOrigin.y;

    // 워프 직전 델타 저장
    ImVec2 currentDelta = io.MouseDelta;

    // 경계 체크 및 워프 (여유 공간 10px)
    const int margin = 10;
    bool wrapped = false;
    POINT newScreenPos = screenPos;

    if (localX <= margin)
    {
        newScreenPos.x = clientOrigin.x + clientW - margin - 1;
        wrapped = true;
    }
    else if (localX >= clientW - margin)
    {
        newScreenPos.x = clientOrigin.x + margin + 1;
        wrapped = true;
    }

    if (localY <= margin)
    {
        newScreenPos.y = clientOrigin.y + clientH - margin - 1;
        wrapped = true;
    }
    else if (localY >= clientH - margin)
    {
        newScreenPos.y = clientOrigin.y + margin + 1;
        wrapped = true;
    }

    if (wrapped)
    {
        // 워프 직전 위치 저장
        lastMousePosBeforeWarp = io.MousePos;

        // 커서 워프
        SetCursorPos(newScreenPos.x, newScreenPos.y);

        // ImGui에게 새 위치 알리기 (클라이언트 좌표로)
        io.MousePos = ImVec2(
            (float)(newScreenPos.x - clientOrigin.x),
            (float)(newScreenPos.y - clientOrigin.y)
        );

        // 중요: Delta는 유지! 워프는 화면상 위치만 바꿀 뿐
        // 실제 마우스 이동량은 그대로 전달되어야 함
        io.MouseDelta = currentDelta;

        // WantSetMousePos로 ImGui에게 위치 변경 알림
        io.WantSetMousePos = true;
    }
    else if (lastMousePosBeforeWarp.x >= 0)
    {
        // 워프 직후 첫 프레임
        // 이때 Delta가 비정상적으로 클 수 있으므로 보정
        ImVec2 expectedDelta = ImVec2(
            io.MousePos.x - lastMousePosBeforeWarp.x,
            io.MousePos.y - lastMousePosBeforeWarp.y
        );

        // Delta가 비정상적으로 크면 (화면 반대편으로 점프) 무시
        float deltaLen = sqrtf(expectedDelta.x * expectedDelta.x +
            expectedDelta.y * expectedDelta.y);
        if (deltaLen > 100.0f) // threshold
        {
            io.MouseDelta = ImVec2(0, 0);
        }

        lastMousePosBeforeWarp = ImVec2(-1, -1);
    }
}

bool MMMEngine::Editor::ImGuiEditorContext::Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_hWnd = hWnd;
    m_pD3DDevice = pDevice;
    m_pD3DContext = pContext;

    // 코어 컨텍스트 생성
    if (!ImGui::CreateContext()) {
        return false;
    }
    m_isImGuiInit = true;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // 중요: 기본 폰트를 먼저 추가
    m_defaultFont = io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/malgun.ttf",
        16.0f,
        NULL,
        io.Fonts->GetGlyphRangesKorean()
    );

    // Font Awesome를 MergeMode로 추가 (기본 폰트에 병합)
    const char* engineDirEnv = std::getenv("MMMENGINE_DIR");
    std::string engineDir = engineDirEnv ? std::string(engineDirEnv) : "";

    if (!engineDir.empty())
    {
        std::string iconFontPath = engineDir + "/Common/Resources/fontawesome_free_solid.otf";

        // 파일 존재 확인
        if (std::filesystem::exists(iconFontPath))
        {
            static const ImWchar icons_ranges[] = { 0xf000, 0xf3ff, 0 };
            ImFontConfig icons_config;
            icons_config.MergeMode = true;  // 기존 폰트에 병합
            icons_config.PixelSnapH = true;
            icons_config.GlyphMinAdvanceX = 16.0f; // 아이콘 최소 너비

            io.Fonts->AddFontFromFileTTF(
                iconFontPath.c_str(),
                16.0f,
                &icons_config,
                icons_ranges
            );
        }
        else
        {
            // 폰트 파일이 없을 때 경고 로그
            // TODO: 로그 출력
        }
    }

    // 큰 폰트는 별도로 추가 (MergeMode 아님)
    m_bigFont = io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/malgun.ttf",
        36.0f,
        NULL,
        io.Fonts->GetGlyphRangesKorean()
    );

    // 스타일 설정
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 6.0f;
    style.WindowRounding = 10.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;

    // Win32 백엔드 초기화
    if (!ImGui_ImplWin32_Init(m_hWnd)) {
        ImGui::DestroyContext();
        return false;
    }
    m_isWin32BackendInit = true;

    // DirectX 11 백엔드 초기화
    if (!ImGui_ImplDX11_Init(m_pD3DDevice, m_pD3DContext)) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    m_isD3D11BackendInit = true;

    ConsoleWindow::Get().Init();
    return true;
}

void MMMEngine::Editor::ImGuiEditorContext::BeginFrame()
{
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);

    // 너비와 높이를 명시적으로 계산
    LONG width = clientRect.right - clientRect.left;
    LONG height = clientRect.bottom - clientRect.top;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)width, (float)height); // float으로 형변환

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void MMMEngine::Editor::ImGuiEditorContext::Render()
{
    // 1. 메인 뷰포트 정보 가져오기
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    // 2. 배경 윈도우 스타일 설정 (테두리와 라운딩 제거)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    // 3. 배경 창을 위한 플래그 설정 (메뉴바 포함 필수)
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;


    // 4. 배경 창 시작
    bool p_open = true;
    ImGui::Begin("MyMainDockSpaceWindow", &p_open, window_flags);
    ImGui::PopStyleVar(3); // StyleVar 2개(Rounding, BorderSize) 복구

    // 5. DockSpace 생성
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_NoCloseButton);
    }

    // 6. 메뉴바 구현
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu(u8"파일"))
        {
            if (ImGui::MenuItem(u8"씬 관리"))
            {
                g_editor_window_scenelist = true;
                p_open = false;
            }
            if (ImGui::MenuItem(u8"씬 저장"))
            {
                auto sceneRef = SceneManager::Get().GetCurrentScene();
                auto sceneRaw = SceneManager::Get().GetSceneRaw(sceneRef);

                SceneSerializer::Get().Serialize(*sceneRaw, SceneManager::Get().GetSceneListPath() + L"/" + StringHelper::StringToWString(sceneRaw->GetName()) + L".scene");
                SceneSerializer::Get().ExtractScenesList(SceneManager::Get().GetAllSceneToRaw(), SceneManager::Get().GetSceneListPath());
                p_open = false;
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(u8"창"))
        {
            ImGui::MenuItem(u8"콘솔", nullptr, &g_editor_window_console);
            ImGui::MenuItem(u8"하이어라키", nullptr, &g_editor_window_hierarchy);
            ImGui::MenuItem(u8"인스펙터", nullptr, &g_editor_window_inspector);
            ImGui::MenuItem(u8"파일 뷰어", nullptr, &g_editor_window_files);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(u8"빌드"))
        {
            if(ImGui::MenuItem(u8"스크립트 빌드"))
            {
                ScriptBuildWindow::Get().StartBuild();
            }
            ImGui::MenuItem(u8"프로젝트 빌드");
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End(); // MyMainDockSpaceWindow 종료

    if (!g_editor_project_loaded)
    {
        StartUpProjectWindow::Get().Render();
        return;
    }

    auto& style = ImGui::GetStyle();

    style.FrameRounding = 6.0f;
    style.WindowRounding = 4.5f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowMenuButtonPosition = ImGuiDir_None;
       
    ConsoleWindow::Get().Render();
    FilesWindow::Get().Render();
    ScriptBuildWindow::Get().Render();
    SceneListWindow::Get().Render();
    HierarchyWindow::Get().Render();
    InspectorWindow::Get().Render();
}

void MMMEngine::Editor::ImGuiEditorContext::EndFrame()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void MMMEngine::Editor::ImGuiEditorContext::Uninitialize()
{
    ConsoleWindow::Get().Shutdown();

    if (m_isD3D11BackendInit)
        ImGui_ImplDX11_Shutdown();
    if (m_isWin32BackendInit)
        ImGui_ImplWin32_Shutdown();
    if (m_isImGuiInit)
        ImGui::DestroyContext();

    m_isImGuiInit = false;
    m_isWin32BackendInit = false;
    m_isD3D11BackendInit = false;
}

bool MMMEngine::Editor::ImGuiEditorContext::GetIsHovered()
{
    return m_isHovered;
}

void MMMEngine::Editor::ImGuiEditorContext::HandleWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (m_isWin32BackendInit)
    {
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
    }
}

