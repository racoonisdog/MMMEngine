#include "InspectorWindow.h"
#include "SceneManager.h"
#include "Transform.h"
#include "Resource.h"

#include "EditorRegistry.h"
using namespace MMMEngine::EditorRegistry;
using namespace DirectX::SimpleMath;
using namespace DirectX;
#include "DragAndDrop.h"
#include "StringHelper.h"
#include "ProjectManager.h"
#include "ResourceManager.h"

#include <optional>

using namespace MMMEngine;
using namespace MMMEngine::Editor;
using namespace MMMEngine::Utility;

static Vector3 g_eulerCache;
static ObjPtr<GameObject> g_lastSelected = nullptr;
static std::vector<ObjPtr<Component>> g_pendingRemoveComponents;
static std::vector<rttr::type> g_componentTypes;
static std::unordered_map<std::string, std::string> g_stringEditCache;

static std::string MakeStringKey(const rttr::instance& inst, const rttr::property& prop)
{
    // 간단히: 타입명 + 프로퍼티명 (인스턴스별 구분까지 하려면 주소/ID 추가 권장)
    auto tname = inst.get_derived_type().get_name().to_string();
    auto pname = prop.get_name().to_string();
    return tname + "::" + pname;
}

void RefreshComponentTypes()
{
    g_componentTypes.clear();

    rttr::type componentType = rttr::type::get<Component>();

    for (const rttr::type& t : rttr::type::get_types())
    {
        // 포인터/기본형/컨테이너 등은 제외하고, "클래스/구조체(record)"만
        if (!t.is_class())
            continue;

        rttr::variant md = t.get_metadata("INSPECTOR");
        if (md.is_valid() && md.is_type<std::string>() && "DONT_ADD_COMP" == md.get_value<std::string>())
            continue;

        if (t != componentType && t.is_derived_from(componentType))
        {
            g_componentTypes.push_back(t);
        }
    }
}

void RenderProperties(rttr::instance inst)
{
    static ObjPtr<GameObject> s_lastCachedObject = nullptr;
    static std::unordered_map<std::string, std::string> cache;
    if (s_lastCachedObject != g_selectedGameObject)
    {
        static std::unordered_map<std::string, std::string> emptyCache;
        cache.swap(emptyCache); // 캐시 클리어
        s_lastCachedObject = g_selectedGameObject;
    }

    auto t = inst.get_derived_type();
    rttr::property p = t.get_property("MUID");
    if (p.is_valid())
    {
        auto v = p.get_value(inst);
        if (v.is_valid() && v.is_type<MUID>())
            ImGui::PushID(v.get_value<MUID>().ToStringWithoutHyphens().c_str());
        else
            ImGui::PushID(inst.try_convert<void*>()); // 주소 기반(예시)
    }
    else
    {
        ImGui::PushID(inst.try_convert<void*>());
    }

    for (auto& prop : t.get_properties())
    {
        rttr::variant md = prop.get_metadata("INSPECTOR");
        if (md.is_valid() && md.is_type<std::string>() && "HIDDEN" == md.get_value<std::string>())
            continue;

        rttr::variant var = prop.get_value(inst);
        if (!var.is_valid())
            continue; // 읽기 불가(접근정책/등록문제 등)


        const std::string name = prop.get_name().to_string();
        const bool readOnly = prop.is_readonly();
        rttr::type propType = prop.get_type();

        if (var.is_type<Vector3>())
        {
            Vector3 v = var.get_value<Vector3>();
            float data[3] = { v.x, v.y, v.z };
            auto SnapToZero = [](float& v, float eps = 1e-4f)
                {
                    if (fabsf(v) < eps) v = 0.0f; // +0로 만들어짐
                };
            SnapToZero(data[0]);
            SnapToZero(data[1]);
            SnapToZero(data[2]);

            if (readOnly) ImGui::BeginDisabled(true);
            bool changed = ImGui::DragFloat3(name.c_str(), data, 0.1f);
            if (readOnly) ImGui::EndDisabled();

            if (changed && !readOnly)
                prop.set_value(inst, Vector3(data[0], data[1], data[2]));
        }
        else if (propType.is_enumeration())
        {
            rttr::enumeration enumType = propType.get_enumeration();
            std::string currentName = enumType.value_to_name(var).to_string();

            if (readOnly) ImGui::BeginDisabled(true);

            if (ImGui::BeginCombo(name.c_str(), currentName.c_str()))
            {
                auto enumNames = enumType.get_names();
                for (const auto& enumName : enumNames)
                {
                    std::string enumNameStr = enumName.to_string();
                    bool isSelected = (enumNameStr == currentName);

                    if (ImGui::Selectable(enumNameStr.c_str(), isSelected))
                    {
                        if (!readOnly)
                        {
                            rttr::variant newValue = enumType.name_to_value(enumName);
                            if (newValue.is_valid())
                            {
                                prop.set_value(inst, newValue);
                            }
                        }
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (readOnly) ImGui::EndDisabled();
        }

        else if (var.is_type<float>())
        {
            float f = var.get_value<float>();

            if (readOnly) ImGui::BeginDisabled(true);
            bool changed = ImGui::DragFloat(name.c_str(), &f);
            if (readOnly) ImGui::EndDisabled();

            if (changed && !readOnly)
                prop.set_value(inst, f);
        }
        else if (var.is_type<bool>())
        {
            bool b = var.get_value<bool>();

            if (readOnly) ImGui::BeginDisabled(true);
            bool changed = ImGui::Checkbox(name.c_str(), &b);
            if (readOnly) ImGui::EndDisabled();

            if (changed && !readOnly)
                prop.set_value(inst, b);
        }
        else if (var.is_type<int>())
        {
            int ntger = var.get_value<int>();

            if (readOnly) ImGui::BeginDisabled(true);
            bool changed = ImGui::DragInt(name.c_str(), &ntger);
            if (readOnly) ImGui::EndDisabled();

            if (changed && !readOnly)
                prop.set_value(inst, ntger);
        }
        else if (propType.get_name().to_string().find("shared_ptr") != std::string::npos)
        {
            auto args = propType.get_template_arguments();
            if (args.begin() != args.end())
            {
                rttr::type innerType = *args.begin();

                if (innerType.is_derived_from(rttr::type::get<Resource>()) ||
                    innerType == rttr::type::get<Resource>())
                {
                    // 현재 리소스
                    Resource* res = nullptr;
                    auto sharedRes = var.get_value<std::shared_ptr<Resource>>();
                    if (sharedRes)
                        res = sharedRes.get();

                    // 표시용 경로
                    std::string displayPath = "None";
                    if (res)
                    {
                        std::wstring fullPath = res->GetFilePath();
                        if (!fullPath.empty())
                        {
                            displayPath = ProjectManager::Get().ToProjectRelativePath(
                                StringHelper::WStringToString(fullPath)
                            );
                        }
                    }

                    // 프로퍼티 이름
                    ImGui::Text("%s:", name.c_str());

                    // 경로 표시 (클릭 가능하게)
                    ImGui::PushID(name.c_str());
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));
                    ImGui::Button(displayPath.c_str(), ImVec2(-1, 0));
                    ImGui::PopStyleColor();

                    // Drag & Drop
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PATH"))
                        {
                            std::string absolutePath((const char*)payload->Data, payload->DataSize - 1);

                            // 파일 확장자 검증 (선택사항)
                            std::string ext = std::filesystem::path(absolutePath).extension().string();

                            // TODO: 리소스 타입별 허용 확장자 체크
                            // 예: StaticMesh -> .mesh, Texture -> .png/.jpg 등

                            std::string relativePath = ProjectManager::Get().ToProjectRelativePath(absolutePath);
                            std::wstring wRelativePath = StringHelper::StringToWString(relativePath);

                            try
                            {
                                rttr::variant loadedResource = ResourceManager::Get().Load(innerType, wRelativePath);

                                if (loadedResource.is_valid())
                                {
                                    if (!readOnly)
                                    {
                                        if (loadedResource.convert(prop.get_type()))                  // v를 내부적으로 target type으로 변환 (bool 리턴)
                                        {
                                            prop.set_value(inst, loadedResource);                     // v는 이제 shared_ptr<StaticMesh> 타입 variant
                                        }
                                        else
                                        {
                                            // 변환 실패 처리
                                        }

                                    }
                                }
                            }
                            catch (const std::exception& e)
                            {
                                auto toUtf8 = [](const char* ansi) -> std::string
                                    {
                                        if (!ansi)
                                            return {};

                                        int wideLen = MultiByteToWideChar(CP_ACP, 0, ansi, -1, nullptr, 0);
                                        if (wideLen <= 0)
                                            return {};

                                        std::wstring wide(wideLen, L'\0');
                                        MultiByteToWideChar(CP_ACP, 0, ansi, -1, wide.data(), wideLen);

                                        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
                                        std::string utf8(utf8Len, '\0');
                                        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), utf8Len, nullptr, nullptr);

                                        return utf8;
                                    };

                                // TODO: 에러 로그
                                std::cerr << "[Resource Load Error]\n"
                                    << "Type: " << innerType.get_name().to_string() << "\n"
                                    << "Path: " << StringHelper::WStringToString(wRelativePath) << "\n"
                                    << "What: " << toUtf8(e.what()) << std::endl;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    // Clear 버튼
                    if (res != nullptr && !readOnly)
                    {
                        ImGui::SameLine();
                        if (ImGui::SmallButton(("X##" + name).c_str()))
                        {
                            std::shared_ptr<Resource> nullPtr = nullptr;
                            prop.set_value(inst, nullPtr);
                        }
                    }

                    ImGui::PopID();
                }
            }
        }
        else if (var.is_type<std::string>())
        {
            // MUID 가져오기
            rttr::property muidProp = t.get_property("MUID");
            rttr::variant muidVar = muidProp.get_value(inst);
            std::string muidStr = "unknown";
            if (muidVar.is_valid() && muidVar.is_type<MMMEngine::Utility::MUID>())
            {
                muidStr = muidVar.get_value<MMMEngine::Utility::MUID>().ToStringWithoutHyphens();
            }

            // 고유 키: MUID + 타입명 + 프로퍼티명
            std::string key = muidStr + "::" + inst.get_type().get_name().to_string() + "::" + name;

            std::string& editing = cache[key];

            if (editing.empty())
                editing = var.get_value<std::string>();

            char buf[256];
            strcpy_s(buf, editing.c_str());

            if (readOnly) ImGui::BeginDisabled(true);
            bool changed = ImGui::InputText(name.c_str(), buf, IM_ARRAYSIZE(buf));
            if (readOnly) ImGui::EndDisabled();

            if (changed && !readOnly)
            {
                editing = buf;
                prop.set_value(inst, editing);
            }
        }

        else if (var.is_type<Quaternion>())
        {
            auto SnapToZero = [](float& v, float eps = 1e-4f) {
                if (fabsf(v) < eps) v = 0.0f;
                };
            // 고유 키 (string 캐시처럼)
            rttr::property muidProp = t.get_property("MUID");
            rttr::variant muidVar = muidProp.get_value(inst);
            std::string muidStr = (muidVar.is_valid() && muidVar.is_type<MMMEngine::Utility::MUID>())
                ? muidVar.get_value<MMMEngine::Utility::MUID>().ToStringWithoutHyphens()
                : "unknown";

            std::string key = muidStr + "::" + inst.get_type().get_name().to_string() + "::" + name;

            // Quaternion용 캐시를 별도로 두는게 깔끔 (static map)
            static std::unordered_map<std::string, Vector3> eulerCache;

            // 1) 실제 값에서 Euler 계산 (도 단위)
            auto q = var.get_value<Quaternion>();
            Vector3 eRad = q.ToEuler(); // (SimpleMath는 보통 rad 반환)
            Vector3 eDeg = { eRad.x * (180.f / XM_PI), eRad.y * (180.f / XM_PI), eRad.z * (180.f / XM_PI) };

            // 2) 캐시 없으면 초기화
            if (eulerCache.find(key) == eulerCache.end())
                eulerCache[key] = eDeg;

            // 3) 인스펙터에서 "현재 이 항목을 편집 중"인지 판별하려면
            //    DragFloat3 호출 후 IsItemActive를 볼 수 있으니,
            //    호출 전에는 "이전 프레임의 상태"가 없어서 보통 이렇게 처리합니다:
            //    - 일단 data를 캐시로 세팅
            //    - DragFloat3 호출 후, Active가 아니고 changed도 아니면 실제 값으로 캐시를 동기화
            float data[3] = { eulerCache[key].x, eulerCache[key].y, eulerCache[key].z };


            if (readOnly) ImGui::BeginDisabled(true);
            bool changed = ImGui::DragFloat3(name.c_str(), data, 0.1f);
            bool active = ImGui::IsItemActive();
            if (readOnly) ImGui::EndDisabled();

            // 4) 사용자가 편집하지 않는 동안엔 gizmo 등 외부 변경을 반영
            if (!active && !changed)
            {
                SnapToZero(eDeg.x);
                SnapToZero(eDeg.y);
                SnapToZero(eDeg.z);
                eulerCache[key] = eDeg; // 외부 변경 반영(= gizmo 최신화)
            }

            // 5) 사용자가 인스펙터에서 편집한 경우만 set_value
            if (changed && !readOnly)
            {

                SnapToZero(data[0]);
                SnapToZero(data[1]);
                SnapToZero(data[2]);

                eulerCache[key] = { data[0], data[1], data[2] };

                Quaternion updatedQ = Quaternion::CreateFromYawPitchRoll(
                    eulerCache[key].y * (XM_PI / 180.f),
                    eulerCache[key].x * (XM_PI / 180.f),
                    eulerCache[key].z * (XM_PI / 180.f)
                );



                updatedQ.Normalize();
                prop.set_value(inst, updatedQ);
            }
        }
        else if (propType.get_name().to_string().find("ResPtr") != std::string::npos)
        {
            Resource* res = nullptr;
            auto sharedRes = var.get_value<std::shared_ptr<Resource>>();
            if (sharedRes)
                res = sharedRes.get();

            std::string displayPath = res ? StringHelper::WStringToString(res->GetFilePath()) : "None";

            // 경로 표시 버튼
            if (ImGui::Button(displayPath.c_str()))
            {
                ImGui::OpenPopup("Select Resource");
            }

            // Drag & Drop 지원
            // None으로 설정 버튼 (X)
        }
    }
    ImGui::PopID();
}

void AddComponentFromDropFilePath(std::string filePath)
{
    if (!filePath.empty())
    {
        std::string ext = Utility::StringHelper::ExtractFileFormat(filePath);
        if (ext != "cpp" && ext != "h")
            return;

        std::string typeName = Utility::StringHelper::ExtractFileName(filePath);
        rttr::type t = rttr::type::get_by_name(typeName);

        if (t.is_valid())
        {
            g_selectedGameObject->AddComponent(t);
        }
    }
}

void MMMEngine::Editor::InspectorWindow::Render()
{
	if (!g_editor_window_inspector)
		return;

	ImGuiWindowClass wc;
	// 핵심: 메인 뷰포트에 이 윈도우를 종속시킵니다.
	// 이렇게 하면 메인 창을 클릭해도 이 창이 '메인 창의 일부'로서 취급되어 우선순위를 가집니다.
	wc.ParentViewportId = ImGui::GetMainViewport()->ID;
	wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing; // 필요 시 설정

	ImGui::SetNextWindowClass(&wc);

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowMenuButtonPosition = ImGuiDir_None;


    static const char* ICON_HIERARCHY = "\xef\x80\x82";

    std::string title = std::string(ICON_HIERARCHY) + u8" 인스펙터";

	ImGui::Begin(title.c_str(), &g_editor_window_inspector);

    // 1. 선택된 게임 오브젝트가 있는지 확인
    if (g_selectedGameObject.IsValid())
    {
        // 2. 오브젝트 이름 출력 및 활성화 상태 체크박스
        char buf[256];
        strcpy_s(buf, g_selectedGameObject->GetName().c_str());
        if (ImGui::InputText("##ObjName", buf, IM_ARRAYSIZE(buf)))
        {
            g_selectedGameObject->SetName(buf);
        }

        ImGui::SameLine();

        bool isActive = g_selectedGameObject->IsActiveSelf();

        if (ImGui::Checkbox(u8"활성화", &isActive))
        {
            g_selectedGameObject->SetActive(isActive);
        }

        char buf2[256];
        strcpy_s(buf2, g_selectedGameObject->GetTag().c_str());
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText(u8"태그", buf2, IM_ARRAYSIZE(buf2)))
        {
            g_selectedGameObject->SetTag(buf2);
        }

        ImGui::SameLine();

        // 현재 오브젝트 레이어 (0~31 가정)
        int curLayer = g_selectedGameObject->GetLayer();   // 없으면 멤버/저장값 쓰세요
        curLayer = (curLayer < 0) ? 0 : (curLayer > 31 ? 31 : curLayer);

        // 미리보기 텍스트
        char preview[8];
        sprintf_s(preview, "%d", curLayer);

        // 폭 지정
        ImGui::SetNextItemWidth(53);

        if (ImGui::BeginCombo(u8"레이어", preview))
        {
            for (int n = 0; n <= 31; ++n)
            {
                bool selected = (n == curLayer);
                if (ImGui::Selectable(std::to_string(n).c_str(), selected))
                {
                    // 선택하는 순간 즉시 반영
                    g_selectedGameObject->SetLayer(n);
                    curLayer = n;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }


        ImGui::Separator();

        g_pendingRemoveComponents.clear();

        // 3. 모든 컴포넌트 순회 및 렌더링
        auto& components = g_selectedGameObject->GetAllComponents();
        int compCount = 0;
        for (auto& comp : components)
        {
            // 각 컴포넌트의 데이터를 ImGui로 출력
            bool visible = true;

            std::string typeName = comp->get_type().get_name().to_string();

           // auto ss = comp->get_type();

            std::string duplicatePrevantName = typeName + "##" + std::to_string(compCount++);
            if (typeName != "Transform")
            {
                if(ImGui::CollapsingHeader(duplicatePrevantName.c_str(), &visible, ImGuiTreeNodeFlags_DefaultOpen))
                    RenderProperties(*comp);
            }
            else if (ImGui::CollapsingHeader(duplicatePrevantName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                RenderProperties(*comp);

            if (!visible)
            {
                visible = true;
                g_pendingRemoveComponents.push_back(comp);
            }
        }

        for (auto& comp : g_pendingRemoveComponents)
        {
            Object::Destroy(comp);
        }

        ImGui::Separator();



        // add component popup
        float width = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button(u8"컴포넌트 추가", ImVec2{ width, 0 }))
        {
            ImGui::OpenPopup(u8"컴포넌트 선택");
        }

        // add component popup
        static char searchBuffer[256] = "";
        int selectedIndex = -1;

        if (ImGui::BeginPopup(u8"컴포넌트 선택"))
        {
            RefreshComponentTypes();

            // 검색 입력 필드
            ImGui::SetNextItemWidth(-1);
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
                searchBuffer[0] = '\0'; // 팝업 열릴 때마다 검색어 초기화
            }
            ImGui::InputTextWithHint("##search", u8"검색...", searchBuffer, IM_ARRAYSIZE(searchBuffer));

            ImGui::Separator();

            // 스크롤 영역 (최대 8개 항목 높이)
            const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
            const float maxHeight = itemHeight * 8;

            ImGui::BeginChild("ComponentList", ImVec2(300, maxHeight), false);

            std::string searchStr = searchBuffer;
            std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

            for (int i = 0; i < g_componentTypes.size(); ++i)
            {
                auto type = g_componentTypes[i];
                std::string typeName = type.get_name().to_string();

                // 검색 필터링
                if (searchStr.length() > 0)
                {
                    std::string lowerTypeName = typeName;
                    std::transform(lowerTypeName.begin(), lowerTypeName.end(), lowerTypeName.begin(), ::tolower);

                    if (lowerTypeName.find(searchStr) == std::string::npos)
                        continue;
                }

                if (ImGui::Selectable(typeName.c_str()))
                {
                    selectedIndex = i;
                    auto selected = g_componentTypes[selectedIndex];
                    g_selectedGameObject->AddComponent(type);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndChild();
            ImGui::EndPopup();
        }

        // script drag and drop area
        ImGui::InvisibleButton("##", ImGui::GetContentRegionAvail());
        std::string file = Editor::GetString("FILE_PATH");
        AddComponentFromDropFilePath(file);
    }
    else
    {
        ImGui::Text(u8"선택된 오브젝트가 없습니다.");
    }

	ImGui::End();
}
