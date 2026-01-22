#include "HierarchyWindow.h"
#include "SceneManager.h"
#include "Transform.h"
#include "DragAndDrop.h"
#include "EditorRegistry.h"
using namespace MMMEngine::EditorRegistry;

#include <optional>

using namespace MMMEngine;
using namespace MMMEngine::Utility;
using namespace MMMEngine::Editor;

struct ReparentCmd
{
	MUID child;
	std::optional<MUID> parent; // nullopt => root(nullptr)
};

static std::vector<ReparentCmd> g_reparentQueue;

// 하이어라키 윈도우 내부에서만 사용할 임시 선택 오브젝트
static ObjPtr<GameObject> g_hierarchyPendingSelection = nullptr;

void DrawDropLine(const char* id, float height = 4.0f)
{
	ImVec2 size(ImGui::GetContentRegionAvail().x, height);
	ImGui::InvisibleButton(id, size);

	if (ImGui::IsItemHovered())
	{
		auto* draw = ImGui::GetWindowDrawList();
		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();

		draw->AddLine(
			ImVec2(min.x, (min.y + max.y) * 0.5f),
			ImVec2(max.x, (min.y + max.y) * 0.5f),
			IM_COL32(80, 160, 255, 255),
			2.0f
		);
	}

	// 여기서 드롭 처리
	MUID dragged = GetMuid("gameobject_muid");
	if (dragged.IsValid() && ImGui::IsItemHovered())
	{
		g_reparentQueue.push_back({ dragged, std::nullopt /* or target */ });
	}
}

void DrawHierarchyMember(ObjPtr<GameObject> obj, bool allowDrag)
{
	auto sceneRef = SceneManager::Get().GetCurrentScene();

	MUID muid = obj->GetMUID();
	ImGui::PushID(muid.ToString().c_str());

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;

	if (obj->GetTransform()->GetChildCount() == 0) flags |= ImGuiTreeNodeFlags_Leaf;
	if (g_selectedGameObject == obj) flags |= ImGuiTreeNodeFlags_Selected;

	bool activeSelf = obj->IsActiveInHierarchy();

	if (!activeSelf)
	{
		ImVec4 c = ImGui::GetStyleColorVec4(ImGuiCol_Text);
		c.w *= 0.35f; // 알파만 낮춰서 흐릿하게
		ImGui::PushStyleColor(ImGuiCol_Text, c);
	}

	bool open = ImGui::TreeNodeEx(obj->GetName().c_str(), flags);

	if (!activeSelf)
		ImGui::PopStyleColor();

	// 클릭 시 임시 선택 오브젝트에만 저장 (전역 레지스트리는 변경하지 않음)
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		g_hierarchyPendingSelection = obj;
	}

	if (allowDrag)
	{
		GiveMuid("gameobject_muid", muid, obj->GetName().c_str());

		MUID dragged_muid = GetMuid("gameobject_muid");
		if (dragged_muid.IsValid())
		{
			auto dragged = SceneManager::Get().FindWithMUID(sceneRef, dragged_muid);
			if (dragged.IsValid())
				g_reparentQueue.push_back({ dragged_muid, obj->GetMUID() });
		}
	}

	if (open)
	{
		auto pt = obj->GetTransform();
		auto child_size = pt->GetChildCount();

		for (size_t i = 0; i < child_size; ++i)
		{
			DrawHierarchyMember(pt->GetChild(i)->GetGameObject(), allowDrag);
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}

void MMMEngine::Editor::HierarchyWindow::Render()
{
	if (!g_editor_window_hierarchy)
		return;

	ImGuiWindowClass wc;
	// 핵심: 메인 뷰포트에 이 윈도우를 종속시킵니다.
	// 이렇게 하면 메인 창을 클릭해도 이 창이 '메인 창의 일부'로서 취급되어 우선순위를 가집니다.
	wc.ParentViewportId = ImGui::GetMainViewport()->ID;
	wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing; // 필요 시 설정

	ImGui::SetNextWindowClass(&wc);

	auto sceneRef = SceneManager::Get().GetCurrentScene();
	auto sceneRaw = SceneManager::Get().GetSceneRaw(sceneRef);
	auto ddolGos = SceneManager::Get().GetAllGameObjectInDDOL();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowMenuButtonPosition = ImGuiDir_None;

	static const char* ICON_HIERARCHY = "\xef\x83\x8a";

	std::string title = std::string(ICON_HIERARCHY) + u8" 하이어라키";

	ImGui::Begin(title.c_str(), &g_editor_window_hierarchy);

	auto hbuttonsize = ImVec2{ ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemSpacing.x / 2, 0 };

	if (ImGui::Button(u8"생성", hbuttonsize))
	{
		Object::NewObject<GameObject>();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(g_selectedGameObject == nullptr);
	if (ImGui::Button(u8"파괴", hbuttonsize)) { Object::Destroy(g_selectedGameObject); g_selectedGameObject = nullptr; }
	ImGui::EndDisabled();

	ImGui::Separator();

	std::string showSceneName = sceneRaw->GetName() + ".scene";
	if (ImGui::CollapsingHeader(showSceneName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		const auto& gameObjects = SceneManager::Get().GetAllGameObjectInCurrentScene();

		for (auto& go : gameObjects)
		{
			if (go->GetTransform()->GetParent() == nullptr)
			{
				DrawHierarchyMember(go, true);
			}
		}

		if (!ddolGos.empty())
			DrawDropLine("##drop_lastline");
		else
		{
			float windowWidth = ImGui::GetContentRegionAvail().x;
			windowWidth = std::max(0.01f, windowWidth);

			ImGui::InvisibleButton("##", { windowWidth,ImGui::GetContentRegionAvail().y });
			MUID muid = GetMuid("gameobject_muid");
			if (muid.IsValid())
			{
				g_reparentQueue.push_back({ muid, std::nullopt }); // root
			}
		}
	}


	if (!ddolGos.empty() && ImGui::CollapsingHeader(u8"Dont Destroy On Load", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (auto& go : ddolGos)
		{
			if (go->GetTransform()->GetParent() == nullptr) DrawHierarchyMember(go, false);
		}
	}

	// 마우스 버튼을 뗐을 때 처리 (ImGui::End() 전에 체크해야 함)
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		// 현재 마우스 위치가 하이어라키 윈도우 내부인지 직접 체크
		ImVec2 mousePos = ImGui::GetMousePos();
		ImVec2 winMin = ImGui::GetWindowPos();
		ImVec2 winMax = ImVec2(winMin.x + ImGui::GetWindowSize().x, winMin.y + ImGui::GetWindowSize().y);

		bool isMouseInWindow = (mousePos.x >= winMin.x && mousePos.x <= winMax.x &&
			mousePos.y >= winMin.y && mousePos.y <= winMax.y);

		// 마우스를 뗀 위치가 하이어라키 윈도우 내부인 경우에만 선택 적용
		if (isMouseInWindow && g_hierarchyPendingSelection.IsValid())
		{
			g_selectedGameObject = g_hierarchyPendingSelection;
		}

		// 임시 선택 오브젝트 초기화
		g_hierarchyPendingSelection = nullptr;
	}

	ImGui::End();

	if (!g_reparentQueue.empty())
	{
		for (const auto& cmd : g_reparentQueue)
		{
			auto childObj = SceneManager::Get().FindWithMUID(sceneRef, cmd.child);
			if (!childObj.IsValid()) continue;

			if (!cmd.parent.has_value())
			{
				childObj->GetTransform()->SetParent(nullptr);
			}
			else
			{
				auto parentObj = SceneManager::Get().FindWithMUID(sceneRef, *cmd.parent);
				if (!parentObj.IsValid()) continue;

				childObj->GetTransform()->SetParent(parentObj->GetTransform());
			}
		}

		g_reparentQueue.clear();
	}
}