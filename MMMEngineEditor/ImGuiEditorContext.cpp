#define NOMINMAX
#include "ImGuiEditorContext.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <ImGuizmo.h>

#include "SceneManager.h"
#include "SceneSerializer.h"
#include "ProjectManager.h"
#include "PhysxManager.h"
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
#include "SceneViewWindow.h"
#include "GameViewWindow.h"
#include "PhysicsSettingsWindow.h"
#include "PlayerBuildWindow.h"
#include "SceneNameWindow.h"
#include "SceneChangeWindow.h"
#include "AssimpLoaderWindow.h"

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

    io.ConfigWindowsMoveFromTitleBarOnly = true;

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
    SceneViewWindow::Get().Initialize(pDevice, pContext, 800, 600);
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
    ImGuizmo::BeginFrame();
}

void MMMEngine::Editor::ImGuiEditorContext::Render()
{

    if (!g_editor_project_loaded)
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImDrawList* dl = ImGui::GetBackgroundDrawList(vp);

        ImU32 bgColor = IM_COL32(30, 30, 30, 255); // 어두운 회색
        dl->AddRectFilled(
            vp->Pos,
            ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
            bgColor
        );
        StartUpProjectWindow::Get().Render();
        return;
    }


    // 1. 메인 뷰포트 정보 가져오기
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);



    // 2. 배경 윈도우 스타일 설정 (테두리와 라운딩 제거)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

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

    ImGui::PopStyleColor(1);

    // 메뉴바 렌더링 흰색으로
    {
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 3.0f);
        //   기본 색상 (흰색 배경, 검정 텍스트, ImGui 기본 강조색)
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));        // 배경: 흰색
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));         // 자식 배경: 흰색
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));         // 팝업 배경: 흰색
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));          // 테두리: 연한 회색
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));      // 프레임 배경: 연한 회색
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.85f, 0.85f, 0.85f, 1.0f)); // 프레임 호버: 약간 어두운 회색
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));  // 프레임 활성: 더 어두운 회색
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));         // 타이틀 배경
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));   // 타이틀 활성
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0.95f, 0.95f, 0.95f, 1.0f)); // 타이틀 축소
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));  // 스크롤바 배경
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));   // 스크롤바 잡기
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // 스크롤바 호버
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));  // 스크롤바 활성
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));            // 텍스트: 검정

        //   선택/강조색은 ImGui 기본 파란색 유지
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));      // 헤더
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.85f, 0.85f, 0.85f, 1.0f)); // 헤더 호버
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));  // 헤더 활성
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.40f));       // 버튼
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 1.00f)); // 버튼 호버
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06f, 0.53f, 0.98f, 1.00f));  // 버튼 활성



        // 6. 메뉴바 구현
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu(u8"파일"))
            {
                if (ImGui::MenuItem(u8"새 프로젝트"))
                {
                    p_open = false;
                }
                if (ImGui::MenuItem(u8"프로젝트 불러오기"))
                {
                    p_open = false;
                }
                if (ImGui::MenuItem(u8"프로젝트 저장"))
                {
                    ProjectManager::Get().SetLastSceneIndex(SceneManager::Get().GetCurrentScene().id);
                    ProjectManager::Get().SaveActiveProject();
                    p_open = false;
                }
                ImGui::Separator();
                if (ImGui::MenuItem(u8"씬 리스트"))
                {
                    g_editor_window_scenelist = true;
                    p_open = false;
                }
                if (ImGui::MenuItem(u8"씬 저장"))
                {
                    auto sceneRef = SceneManager::Get().GetCurrentScene();
                    auto sceneRaw = SceneManager::Get().GetSceneRaw(sceneRef);

                    SceneSerializer::Get().Serialize(*sceneRaw, SceneManager::Get().GetSceneListPath() + L"/" + StringHelper::StringToWString(sceneRaw->GetName()) + L".scene");
                    SceneManager::Get().ReloadSnapShotCurrentScene();
                    SceneSerializer::Get().ExtractScenesList(SceneManager::Get().GetAllSceneToRaw(), SceneManager::Get().GetSceneListPath());
                    p_open = false;
                }
                if (ImGui::MenuItem(u8"씬 이름 변경"))
                {
                    g_editor_window_sceneName = true;
                    p_open = false;
                }
                if (ImGui::MenuItem(u8"씬 전환"))
                {
                    g_editor_window_sceneChange = true;
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
                ImGui::MenuItem(u8"씬", nullptr, &g_editor_window_sceneView);
                ImGui::MenuItem(u8"게임", nullptr, &g_editor_window_gameView);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(u8"도구"))
            {
                if (ImGui::MenuItem(u8"스크립트 빌드"))
                {
                    ScriptBuildWindow::Get().StartBuild();
                }
                if (ImGui::MenuItem(u8"프로젝트 빌드"))
                {
                    g_editor_window_playerBuild = true;
					p_open = false;
                }
                ImGui::Separator();
                if (ImGui::MenuItem(u8"물리 설정"))
                {
                    g_editor_window_physicsSettings = true;
                    p_open = false;
                }
                if (ImGui::MenuItem(u8"Assimp 로더"))
                {
                    g_editor_window_assimpLoader = true;
                    p_open = false;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }


        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));


        {
            const float line_h = 1.0f; // 정확히 1픽셀
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // 현재 커서 위치 (메뉴바 바로 아랫단)
            ImVec2 p = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;

            // 회색 라인 그리기 (IM_COL32의 마지막 인자는 알파값 255)
            dl->AddRectFilled(p, ImVec2(p.x + w, p.y + line_h), IM_COL32(180, 180, 180, 255));

            // 커서를 1px만큼만 정확히 이동시켜서 DockSpace가 이 선 바로 밑에 붙게 함
            ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + line_h));
        }

        // 2) 짙은 색 테마 복구 (PopStyleColor 21개)
        ImGui::PopStyleColor(21);


        // 1. 툴바 스타일 및 배경 설정
        ImVec4 toolbarBg = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, toolbarBg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f)); // 버튼 사이 간격 4px (적당함)

        static int g_fpsDisplayMode = 0;

        // --- 높이를 32px로 설정 (너무 좁지도, 넓지도 않은 표준 높이) ---
        bool openPopup = false;
        float toolbarHeight = 32.0f;
        if (ImGui::BeginChild("EditorToolbar", ImVec2(0, toolbarHeight), false, ImGuiWindowFlags_NoScrollbar))
        {
            float logoBtnWidth = 85.0f;  // 아이콘 + "MMM" 텍스트를 담기에 충분한 너비
            float logoBtnHeight = 24.0f; // 중앙 버튼들과 비슷한 높이 (또는 살짝 작게)

            // 왼쪽 여백 10px, 수직 중앙 정렬
            ImGui::SetCursorPos(ImVec2(10.0f, (toolbarHeight - logoBtnHeight) * 0.5f));

            // 로고 버튼 전용 스타일 (중앙 버튼과 통일감 부여)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f)); // 하늘색 로고 텍스트

            // 빈 버튼을 먼저 생성 (ID만 부여)
            if (ImGui::Button("##AboutLogo", ImVec2(logoBtnWidth, logoBtnHeight)))
            {
                openPopup = true;
            }

            // --- 버튼 내부 아이콘+텍스트 수직 정렬을 위한 수동 렌더링 ---
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // 아이콘과 텍스트가 정중앙에 오도록 좌표 계산
            const char* logoStr = (const char*)u8"\uf1b2 MMM";
            ImVec2 textSize = ImGui::CalcTextSize(logoStr);

            // 버튼의 중앙 좌표에 텍스트 절반 크기를 빼서 정렬
            ImVec2 textPos = ImVec2(
                min.x + (logoBtnWidth - textSize.x) * 0.5f,
                min.y + (logoBtnHeight - textSize.y) * 0.5f // +1px은 폰트 베이스라인 보정용
            );

            dl->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), logoStr);

            ImGui::PopStyleColor(1);

            // --- 버튼 내부 여백 및 라운딩 (심미성 강화) ---
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

            float buttonWidth = 40.0f;
            float buttonHeight = 26.0f; // 툴바(32) 내에서 상하 3px씩 여백이 남는 크기
            float totalWidth = (buttonWidth * 3) + (ImGui::GetStyle().ItemSpacing.x * 2);

            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - totalWidth) * 0.5f);
            // 수직 중앙 정렬: (32 - 26) / 2 = 3px
            ImGui::SetCursorPosY((toolbarHeight - buttonHeight) * 0.5f);

            // --- [Play / Stop] ---
            const char* playIcon = g_editor_scene_playing ? (const char*)u8"\uf04d" : (const char*)u8"\uf04b";
            bool isPlaying = g_editor_scene_playing;
            if (isPlaying) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));

            if (ImGui::Button(playIcon, ImVec2(buttonWidth, buttonHeight))) {
                g_editor_scene_playing = !g_editor_scene_playing;
                if (g_editor_scene_playing) g_editor_scene_pause = false;

                if (g_editor_scene_playing)
                {
					g_editor_scene_before_play_sceneID = SceneManager::Get().GetCurrentScene().id;
                    SceneManager::Get().ReloadSnapShotCurrentScene();
                    PhysxManager::Get().SyncRigidsFromTransforms();
                }
                else
                {
                    auto currenSceneRef = SceneManager::Get().GetCurrentScene();
                    SceneManager::Get().ChangeScene(g_editor_scene_before_play_sceneID);
                    SceneManager::Get().ClearDDOLScene();
                }
            }
            if (isPlaying) ImGui::PopStyleColor();

            ImGui::SameLine();

            // --- [Pause] ---
            bool isPaused = g_editor_scene_pause && g_editor_scene_playing;
            if (isPaused) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));

            if (ImGui::Button((const char*)u8"\uf04c", ImVec2(buttonWidth, buttonHeight))) {
                if (g_editor_scene_playing) g_editor_scene_pause = !g_editor_scene_pause;
            }
            if (isPaused) ImGui::PopStyleColor();

            ImGui::SameLine();

            // --- [Next Frame] ---
            ImGui::Button((const char*)u8"\uf051", ImVec2(buttonWidth, buttonHeight));

            // --- [오른쪽 영역: 상태 정보 및 FPS 카운터] ---
            {
                // 상태 결정은 동일
                ImGuiIO& io = ImGui::GetIO();
                float fps = io.Framerate;

                ImVec4 statusColor;
                const char* statusStr = "";
                if (fps >= 120.0f) { statusStr = u8"양호"; statusColor = ImVec4(1, 1, 1, 1); }
                else if (fps >= 20.0f) { statusStr = u8"나쁨"; statusColor = ImVec4(1, 0.6f, 0, 1); }
                else { statusStr = u8"매우 나쁨"; statusColor = ImVec4(1, 0.2f, 0.2f, 1); }

                // 모드별 표시 문자열
                char displayText[128];
                if (g_fpsDisplayMode == 0) {
                    sprintf_s(displayText, "%.1f FPS (%.2f ms/f)", fps, 1000.0f / fps);
                }
                else {
                    sprintf_s(displayText, u8"프레임 상태 : %s", statusStr);
                }

                // 텍스트 폭 기반 자동 너비
                const float padX = 16.0f;
                float textW = ImGui::CalcTextSize(displayText).x;
                float badgeWidth = textW + padX * 2.0f;
                float badgeHeight = 24.0f;

                ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - badgeWidth - 10.0f,
                    (toolbarHeight - badgeHeight) * 0.5f));

                // 버튼(고정 ID) + 텍스트(별도 draw)
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 size(badgeWidth, badgeHeight);

                bool clicked = ImGui::Button("##FPS_TOGGLE", size);

                ImVec2 tsize = ImGui::CalcTextSize(displayText);
                ImVec2 tpos(p0.x + (size.x - tsize.x) * 0.5f,
                    p0.y + (size.y - tsize.y) * 0.5f);

                ImGui::GetWindowDrawList()->AddText(tpos, ImGui::GetColorU32(statusColor), displayText);

                if (clicked) {
                    g_fpsDisplayMode ^= 1;
                    // 디버그 확인
                    // printf("mode=%d\n", g_fpsDisplayMode);
                }
            }
            ImGui::PopStyleColor(3); // Button 색상들 Pop
            ImGui::PopStyleVar(2);   // FramePadding, FrameRounding Pop
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(2);   // ChildRounding, ItemSpacing Pop
        ImGui::PopStyleColor(1); // ChildBg Pop

        if(openPopup)
            ImGui::OpenPopup(u8"정보");

        // --- 중앙 배치를 위한 설정 ---
        ImVec2 center = ImGui::GetMainViewport()->GetCenter(); // 메인 창의 중심 좌표 가져오기
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
        if (ImGui::BeginPopupModal(u8"정보", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            // 제목 및 엔진 정보
            ImGui::PushFont(m_bigFont); // Initialize에서 만든 큰 폰트 사용 (선택 사항)
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "MMM Engine");
            ImGui::PopFont();

            ImGui::TextDisabled(u8"내수용 엔진 (Reverse WWW)");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // 약간의 간격

            // 기여자 정보 표 형식으로 깔끔하게 정리
            ImGui::Text(u8"기여자 : 4명");
            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            if (ImGui::BeginTable("ContributorsTable", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled(u8"엔진코어 :");
                ImGui::TableSetColumnIndex(1); ImGui::Text(u8" ㅇㅇㅇ");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled(u8"렌더링   :");
                ImGui::TableSetColumnIndex(1); ImGui::Text(u8" ㅇㅇㅇ");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled(u8"물리     :");
                ImGui::TableSetColumnIndex(1); ImGui::Text(u8" ㅇㅇㅇ");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled(u8"사운드   :");
                ImGui::TableSetColumnIndex(1); ImGui::Text(u8" ㅇㅇㅇ");

                ImGui::EndTable();
            }

            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            // 닫기 버튼 (중앙 정렬)
            float buttonWidth = 100.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - buttonWidth) * 0.5f);
            if (ImGui::Button(u8"확인", ImVec2(buttonWidth, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        // 이 아래에 다시 1px 회색 라인을 추가하여 툴바와 도킹 영역을 구분하면 더 깔끔합니다.
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(p, ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + 1.0f), IM_COL32(100, 100, 100, 255));
            ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 1.0f));
        }

        // 3) 이제 이 위치(선 아래)부터 도킹 시작
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

            if (ImGui::DockBuilderGetNode(dockspace_id) == NULL)
            {
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

                ImGuiID dock_main_id = dockspace_id;

                ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(
                    dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id
                );


                ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(
                    dock_main_id, ImGuiDir_Down, 0.3f, nullptr, &dock_main_id
                );

                ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(
                    dock_main_id, ImGuiDir_Left, 0.25f, nullptr, &dock_main_id
                );


                ImGuiID dock_id_bottom_left, dock_id_bottom_right;
                ImGui::DockBuilderSplitNode(
                    dock_id_bottom, ImGuiDir_Left, 0.4f, &dock_id_bottom_left, &dock_id_bottom_right
                );

                // 윈도우 배치
                ImGui::DockBuilderDockWindow(u8"\uf0ca 하이어라키", dock_id_left);
                ImGui::DockBuilderDockWindow(u8"\uf002 인스펙터", dock_id_right);
                ImGui::DockBuilderDockWindow(u8"\uf009 씬", dock_main_id);
                ImGui::DockBuilderDockWindow(u8"\uf11b 게임", dock_main_id);
                ImGui::DockBuilderDockWindow(u8"\uf07c 파일 뷰어", dock_id_bottom_left);
                ImGui::DockBuilderDockWindow(u8"\uf120 콘솔", dock_id_bottom_right);

                ImGui::DockBuilderFinish(dockspace_id);
            }

            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_NoCloseButton);
        }
        // 4) 적용했던 간격 관련 StyleVar 복구
        ImGui::PopStyleVar(3);

    }
    ImGui::End(); // MyMainDockSpaceWindow 종료

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
	SceneNameWindow::Get().Render();
	SceneChangeWindow::Get().Render();
    HierarchyWindow::Get().Render();
    InspectorWindow::Get().Render();
    GameViewWindow::Get().Render();
    SceneViewWindow::Get().Render();
    PhysicsSettingsWindow::Get().Render();
    PlayerBuildWindow::Get().Render();
    AssimpLoaderWindow::Get().Render();
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

