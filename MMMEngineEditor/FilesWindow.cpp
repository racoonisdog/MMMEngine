#include "FilesWindow.h"
#include <vector>
#include <string>
#include <unordered_set>

#include "EditorRegistry.h"
#include "ProjectManager.h"
#include "BuildManager.h"

using namespace MMMEngine::EditorRegistry;

// 또는 unordered_set으로 (검색 성능 향상)
static inline std::unordered_set<std::string> fileExclusionsSet = {
    ".obj", ".csproj", ".vcxproj", ".filters", ".user", ".dll", ".exp", ".lib", ".settings"
};

void MMMEngine::Editor::FilesWindow::Render()
{
    if (!g_editor_window_files)
        return;

    ImGuiWindowClass wc;
    wc.ParentViewportId = ImGui::GetMainViewport()->ID;
    wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing; // 필요 시 설정

    ImGui::SetNextWindowClass(&wc);

    hoveredFileOrDir.clear();

    // Process move queue
    for (const auto& [itemPath, destPath] : m_moveQueue)
    {
        fs::path item(itemPath);
        fs::path dest(destPath);
        fs::path movedPath = dest / item.filename();

        if (itemPath == movedPath.string()) continue;

        try
        {
            if (fs::exists(item))
            {
                fs::rename(item, movedPath);
            }
        }
        catch (const std::exception& e)
        {
            // TODO: Log error
        }
    }
    m_moveQueue.clear();

    ImGui::Begin(u8"\uf07c 파일 뷰어", &g_editor_window_files);

    hovered = ImGui::IsWindowHovered();

    if (ProjectManager::Get().HasActiveProject())
    {
        const auto& project = ProjectManager::Get().GetActiveProject();
        fs::path root = fs::path(project.rootPath);

        // 초기화: 현재 디렉토리가 비어있으면 루트로 설정
        if (m_currentDirectory.empty())
        {
            m_currentDirectory = root;
            m_navigationHistory.clear();
            m_navigationHistory.push_back(m_currentDirectory);
            m_historyIndex = 0;
        }

        DrawToolbar(root);
        ImGui::Separator();
        DrawNavigationBar(root);
        ImGui::Separator();
        DrawGridView(root);
    }

    ImGui::End();
}

void MMMEngine::Editor::FilesWindow::DrawToolbar(const fs::path& root)
{
    float windowVisibleX2 = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;

    // 도우미 함수: 이전 아이템 이후에 다음 아이템이 들어갈 공간이 있는지 확인
    auto ConfirmSameLine = [&](float nextItemWidth) {
        float lastItemX2 = ImGui::GetItemRectMax().x;
        float nextItemX2 = lastItemX2 + spacing + nextItemWidth;
        if (nextItemX2 < windowVisibleX2)
            ImGui::SameLine();
        };

    // 1. 뒤로 가기 (첫 번째 아이템은 SameLine 체크가 필요 없음)
    ImGui::BeginDisabled(m_historyIndex <= 0);
    if (ImGui::Button("\xef\x81\xa0"))
    {
        if (m_historyIndex > 0) {
            m_historyIndex--;
            m_currentDirectory = m_navigationHistory[m_historyIndex];
        }
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"뒤로 가기");

    // 2. 앞으로 가기
    ConfirmSameLine(ImGui::GetFrameHeight());
    ImGui::BeginDisabled(m_historyIndex >= (int)m_navigationHistory.size() - 1);
    if (ImGui::Button("\xef\x81\xa1"))
    {
        if (m_historyIndex < (int)m_navigationHistory.size() - 1) {
            m_historyIndex++;
            m_currentDirectory = m_navigationHistory[m_historyIndex];
        }
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"앞으로 가기");

    // 3. 상위 폴더로
    ConfirmSameLine(ImGui::GetFrameHeight());
    ImGui::BeginDisabled(m_currentDirectory == root);
    if (ImGui::Button("\xef\x81\xa2"))
    {
        if (m_currentDirectory.has_parent_path() && m_currentDirectory != root) {
            NavigateTo(m_currentDirectory.parent_path());
        }
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"상위 폴더로");

    // 4. 아이콘 크기 슬라이더 (그룹화)
    float sliderWidth = 100.0f;
    float sliderGroupWidth = ImGui::CalcTextSize(u8"아이콘 크기:").x + spacing + sliderWidth;
    ConfirmSameLine(sliderGroupWidth);
    ImGui::BeginGroup();
    ImGui::Text(u8"아이콘 크기:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(sliderWidth);
    ImGui::SliderFloat("##iconsize", &m_iconSize, 50.0f, 150.0f, "%.0f");
    ImGui::EndGroup();

    // 5. 새 폴더 버튼
    float newFolderBtnWidth = ImGui::CalcTextSize(u8"새 폴더").x + ImGui::GetStyle().FramePadding.x * 2;
    ConfirmSameLine(newFolderBtnWidth);
    if (ImGui::Button(u8"새 폴더"))
    {
        fs::path newFolderPath = MakeFolderUnique(m_currentDirectory, "folder");
        fs::create_directories(newFolderPath);
    }

    // 6. 삭제 버튼
    float deleteBtnWidth = ImGui::CalcTextSize(u8"삭제").x + ImGui::GetStyle().FramePadding.x * 2;
    ConfirmSameLine(deleteBtnWidth);
    ImGui::BeginDisabled(selectedFileOrDir.empty());
    if (ImGui::Button(u8"삭제"))
    {
        try {
            fs::path toDelete(selectedFileOrDir);
            if (fs::exists(toDelete)) {
                if (fs::is_directory(toDelete)) fs::remove_all(toDelete);
                else fs::remove(toDelete);
                selectedFileOrDir.clear();
            }
        }
        catch (...) {}
    }
    ImGui::EndDisabled();
}
void MMMEngine::Editor::FilesWindow::DrawNavigationBar(const fs::path& root)
{
    // Breadcrumb navigation
    std::vector<fs::path> pathParts;
    fs::path temp = m_currentDirectory;

    while (temp != root && !temp.empty())
    {
        pathParts.push_back(temp);
        if (!temp.has_parent_path()) break;
        temp = temp.parent_path();
    }
    pathParts.push_back(root);
    std::reverse(pathParts.begin(), pathParts.end());

    for (size_t i = 0; i < pathParts.size(); i++)
    {
        if (i > 0)
        {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
        }

        // imgui id push로 파일 안에 같은 이름의 파일이 있더라도 UI ID겹침 방지
        ImGui::PushID(static_cast<int>(i));

        std::string label = (i == 0) ? "Root" : pathParts[i].filename().string();

        if (ImGui::SmallButton(label.c_str()))
        {
            NavigateTo(pathParts[i]);
        }

        // id pop
        ImGui::PopID(); 
    }
}

void MMMEngine::Editor::FilesWindow::DrawGridView(const fs::path& root)

{
    if (!fs::exists(m_currentDirectory) || !fs::is_directory(m_currentDirectory))
        return;

    std::vector<fs::path> directories;
    std::vector<fs::path> files;

    // Collect entries
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(m_currentDirectory, ec))
    {
        if (ec) break;

        if (entry.is_directory())
        {
            std::string name = entry.path().filename().string();
            bool isExcluded = std::find(m_folderRenderExclusions.begin(),
                m_folderRenderExclusions.end(),
                name) != m_folderRenderExclusions.end();

#ifdef _WIN32
            bool isHidden = (entry.status().permissions() & fs::perms::others_exec) == fs::perms::none;
#else
            bool isHidden = name[0] == '.';
#endif

            if (!isExcluded && !isHidden)
                directories.push_back(entry.path());
        }
        else if (entry.is_regular_file())
        {
            std::string ext = entry.path().extension().string();
            bool isExcluded = std::find(m_fileRenderExclusions.begin(),
                m_fileRenderExclusions.end(),
                ext) != m_fileRenderExclusions.end();

            if (!isExcluded)
                files.push_back(entry.path());
        }
    }

    // Sort alphabetically
    std::sort(directories.begin(), directories.end());
    std::sort(files.begin(), files.end());

    // Calculate grid layout
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float cellSize = m_iconSize + m_iconPadding * 2;
    int columns = std::max(1, (int)(windowWidth / cellSize));

    ImGui::BeginChild("GridScrollRegion", ImVec2(0, 0), false);

    // Render grid
    int itemIndex = 0;
    int totalItems = directories.size() + files.size();

    // Draw directories first
    for (const auto& dir : directories)
    {
        DrawGridItem(dir, true, itemIndex, columns);
        itemIndex++;
    }

    // Draw files
    for (const auto& file : files)
    {
        DrawGridItem(file, false, itemIndex, columns);
        itemIndex++;
    }

    // Handle empty space click (deselect)
    ImVec2 availRegion = ImGui::GetContentRegionAvail();
    if (availRegion.x > 0.0f && availRegion.y > 0.0f)
    {
        ImGui::InvisibleButton("##emptyspace", availRegion);
        if (ImGui::IsItemClicked())
        {
            selectedFileOrDir.clear();
        }
    }
        
   

    // Drop target for current directory
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PATH"))
        {
            std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
            m_moveQueue.push_back({ droppedPath, m_currentDirectory.string() });
        }
        ImGui::EndDragDropTarget();
    }

    // Right click menu on empty space
    if (ImGui::BeginPopupContextItem("EmptySpaceRightClickMenu"))
    {
        if (ImGui::MenuItem(u8"새 폴더"))
        {
            fs::path newFolderPath = MakeFolderUnique(m_currentDirectory, "folder");
            fs::create_directories(newFolderPath);
        }

        if (ImGui::MenuItem(u8"새 스크립트"))
        {
            m_openCreateScriptModalRequest = true;
            m_newScriptParentDirectory = m_currentDirectory.string();
        }

        ImGui::EndPopup();
    }

    // Create Script Modal
    if (m_openCreateScriptModalRequest)
    {
        ImGui::OpenPopup(u8"스크립트 생성");
        m_openCreateScriptModalRequest = false;
    }

    ImGui::SetNextWindowSize(ImVec2(350, 0), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(u8"스크립트 생성", nullptr, ImGuiWindowFlags_NoResize))
    {
        ImGui::Text(u8"이름:");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputText("##scriptname", m_newScriptName, sizeof(m_newScriptName));

        bool emptyName = strlen(m_newScriptName) == 0;
        ImGui::BeginDisabled(emptyName);
        if (ImGui::Button(u8"생성"))
        {
            CreateNewScript(m_newScriptParentDirectory, m_newScriptName);
            memset(m_newScriptName, 0, sizeof(m_newScriptName));
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        if (ImGui::Button(u8"취소"))
        {
            memset(m_newScriptName, 0, sizeof(m_newScriptName));
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::EndChild();
}

void MMMEngine::Editor::FilesWindow::DrawGridItem(const fs::path& path, bool isDirectory, int index, int columns)

{
    std::string pathStr = path.string();
    std::string fileName = path.filename().string();

    // Start new row if needed
    if (index % columns != 0)
    {
        ImGui::SameLine();
    }

    ImGui::BeginGroup();
    ImGui::PushID(pathStr.c_str());

    // Calculate cell size
    float cellSize = m_iconSize + m_iconPadding * 2;
    ImVec2 cursorPos = ImGui::GetCursorPos();

    // Selection background
    bool isSelected = selectedFileOrDir == pathStr;
    if (isSelected)
    {
        ImVec2 min = ImGui::GetCursorScreenPos();
        ImVec2 max = ImVec2(min.x + cellSize, min.y + cellSize + 15); // +15 for text
        ImGui::GetWindowDrawList()->AddRectFilled(
            min, max,
            IM_COL32(50, 100, 200, 100),
            4.0f
        );
    }

    // Icon button
    ImGui::SetCursorPos(ImVec2(cursorPos.x + m_iconPadding, cursorPos.y + m_iconPadding));

    // Icon (using Font Awesome or colored square)
    ImVec2 iconMin = ImGui::GetCursorScreenPos();
    ImVec2 iconMax = ImVec2(iconMin.x + m_iconSize, iconMin.y + m_iconSize);

    ImU32 iconColor;
    const char* iconGlyph;

    if (isDirectory)
    {
        iconColor = IM_COL32(255, 200, 80, 255); // Yellow folder
        iconGlyph = "\xef\x81\xbc"; // Folder icon
    }
    else
    {
        std::string ext = path.extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c")
        {
            iconColor = IM_COL32(100, 150, 255, 255); // Blue for code
            iconGlyph = "\xef\x87\x89"; // File-code icon
        }
        else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
        {
            iconColor = IM_COL32(200, 100, 255, 255); // Purple for images
            iconGlyph = "\xef\x87\x85"; // File-image icon
        }
        else
        {
            iconColor = IM_COL32(200, 200, 200, 255); // Gray for others
            iconGlyph = "\xef\x85\x9b"; // File icon
        }
    }

    // Draw icon background
    ImGui::GetWindowDrawList()->AddRectFilled(iconMin, iconMax, iconColor, 8.0f);

    // Draw icon glyph (centered)
// 아이콘 텍스트 크기 계산
    ImVec2 iconTextSize = ImGui::CalcTextSize(iconGlyph);

    // 중앙 정렬 위치 계산
    ImVec2 iconTextPos(
        iconMin.x + (m_iconSize - iconTextSize.x) * 0.5f,
        iconMin.y + (m_iconSize - iconTextSize.y) * 0.5f
    );

    ImGui::SetCursorScreenPos(iconTextPos);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::TextUnformatted(iconGlyph);
    ImGui::PopStyleColor();

    // Invisible button for interaction
    ImGui::SetCursorPos(ImVec2(cursorPos.x + m_iconPadding, cursorPos.y + m_iconPadding));
    ImGui::InvisibleButton("##icon", ImVec2(m_iconSize, m_iconSize));

    // Hover effect
    if (ImGui::IsItemHovered())
    {
        hoveredFileOrDir = pathStr;
        ImGui::GetWindowDrawList()->AddRect(
            iconMin, iconMax,
            IM_COL32(255, 255, 255, 150),
            8.0f, 0, 2.0f
        );
    }

    // Click handling
    if (ImGui::IsItemClicked(0))
    {
        selectedFileOrDir = pathStr;
    }

    // Double click handling
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
    {
        if (isDirectory)
        {
            NavigateTo(path);
        }
        else
        {
            OpenFileInEditor(path);
        }
    }

    // Drag source
    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("FILE_PATH", pathStr.c_str(), pathStr.size() + 1);
        ImGui::Text(u8"%s", fileName.c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target (for directories only)
    if (isDirectory && ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PATH"))
        {
            std::string droppedPath((const char*)payload->Data, payload->DataSize - 1);
            m_moveQueue.push_back({ droppedPath, pathStr });
        }
        ImGui::EndDragDropTarget();
    }

    // Right click menu
    if (ImGui::BeginPopupContextItem("ItemContextMenu"))
    {
        if (ImGui::MenuItem(u8"삭제"))
        {
            try
            {
                if (fs::exists(path))
                {
                    if (fs::is_directory(path))
                        fs::remove_all(path);
                    else
                        fs::remove(path);
                    if (selectedFileOrDir == pathStr)
                        selectedFileOrDir.clear();
                }
            }
            catch (const std::exception& e)
            {
                // TODO: Log error
            }
        }

        if (isDirectory)
        {
            ImGui::Separator();
            if (ImGui::MenuItem(u8"새 폴더"))
            {
                fs::path newFolderPath = MakeFolderUnique(path, "folder");
                fs::create_directories(newFolderPath);
            }

            if (ImGui::MenuItem(u8"새 스크립트"))
            {
                m_openCreateScriptModalRequest = true;
                m_newScriptParentDirectory = pathStr;
            }
        }

        ImGui::EndPopup();
    }

    // File name text (truncated if too long)
    ImGui::SetCursorPos(ImVec2(cursorPos.x, cursorPos.y + m_iconSize + m_iconPadding + 5));

    std::string displayName = fileName;
    if (displayName.length() > 12)
    {
        displayName = displayName.substr(0, 9) + "...";
    }

    float textWidth = ImGui::CalcTextSize(displayName.c_str()).x;
    float textOffset = (cellSize - textWidth) * 0.5f;
    ImGui::SetCursorPosX(cursorPos.x + textOffset);
    ImGui::TextWrapped(u8"%s", displayName.c_str());

    if (ImGui::IsItemHovered() && fileName.length() > 12)
    {
        ImGui::SetTooltip(u8"%s", fileName.c_str());
    }

    ImGui::PopID();
    ImGui::EndGroup();
}

void MMMEngine::Editor::FilesWindow::NavigateTo(const fs::path& newPath)
{
    if (newPath == m_currentDirectory) return;

    m_currentDirectory = newPath;

    // Trim history after current index
    if (m_historyIndex < (int)m_navigationHistory.size() - 1)
    {
        m_navigationHistory.erase(
            m_navigationHistory.begin() + m_historyIndex + 1,
            m_navigationHistory.end()
        );
    }

    m_navigationHistory.push_back(m_currentDirectory);
    m_historyIndex = m_navigationHistory.size() - 1;

    selectedFileOrDir.clear();
}

void MMMEngine::Editor::FilesWindow::CreateNewScript(const std::string& parentDir, const std::string& scriptName)

{
    fs::path scriptsDir = fs::path(parentDir);// / "Source" / "UserScripts" / "Scripts";

    if (!fs::exists(scriptsDir))
    {
        fs::create_directories(scriptsDir);
    }

    // Create .h file
    fs::path headerPath = MakeFileUnique(scriptsDir, scriptName, ".h");
    std::ofstream headerFile(headerPath, std::ios::binary);
    if (headerFile.is_open())
    {
        headerFile <<
            R"(#include "rttr/type"
#include "ScriptBehaviour.h"
#include "UserScriptsCommon.h"

namespace MMMEngine
{
    class USERSCRIPTS )" << scriptName << R"( : public ScriptBehaviour
    {
    private:
        RTTR_ENABLE(ScriptBehaviour)
        RTTR_REGISTRATION_FRIEND
    public:
        )" << scriptName << R"(()
        {
        }

        USCRIPT_MESSAGE()
        void Start();

        USCRIPT_MESSAGE()
        void Update();
    };
}
)";
        headerFile.close();
    }

    // Create .cpp file
    fs::path cppPath = MakeFileUnique(scriptsDir, scriptName, ".cpp");
    std::ofstream cppFile(cppPath, std::ios::binary);
    if (cppFile.is_open())
    {
        cppFile <<
            R"(#include "Export.h"
#include "ScriptBehaviour.h"
#include ")" << scriptName << R"(.h"

void MMMEngine::)" << scriptName << R"(::Start()
{
}

void MMMEngine::)" << scriptName << R"(::Update()
{
}
)";
        cppFile.close();
    }
}

void MMMEngine::Editor::FilesWindow::OpenFileInEditor(const fs::path& filePath)
{
    std::string ext = filePath.extension().string();

    // C++ 소스 파일인 경우 Visual Studio로 열기 (/edit = 기존 창에 열기, 없으면 1회만 실행)
    if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c")
    {
#ifdef _WIN32
        fs::path devenvPath = BuildManager::Get().FindDevEnv();
        if (!devenvPath.empty())
        {
            // /edit: 이미 떠 있는 VS에 파일만 열기. 없으면 VS 한 번 띄우고 그 창에 열림
            std::string cmd = "start \"\" \"" + devenvPath.string() + "\" /edit \"" + filePath.string() + "\"";
            int result = system(cmd.c_str());
            if (result == 0)
                return;
        }
#endif
    }

    // 기본 에디터로 열기 (폴백)
#ifdef _WIN32
    std::string cmd = "code \"" + filePath.string() + "\"";
    int result = system(cmd.c_str());
    if (result != 0)
    {
        cmd = "notepad \"" + filePath.string() + "\"";
        system(cmd.c_str());
    }
#else
    std::string cmd = "xdg-open \"" + filePath.string() + "\"";
    system(cmd.c_str());
#endif
}

fs::path MMMEngine::Editor::FilesWindow::MakeFileUnique(const fs::path& parentDir, const std::string& fileName, const std::string& extension)
{
    fs::path path = parentDir / (fileName + extension);

    for (int i = 1; i < 100; i++)
    {
        if (!fs::exists(path))
            break;
        path = parentDir / (fileName + "_" + std::to_string(i) + extension);
    }

    return path;
}

fs::path MMMEngine::Editor::FilesWindow::MakeFolderUnique(const fs::path& parentDir, const std::string& folderName)
{
    fs::path path = parentDir / folderName;

    for (int i = 1; i < 100; i++)
    {
        if (!fs::exists(path))
            break;
        path = parentDir / (folderName + "_" + std::to_string(i));
    }

    return path;
}
