#pragma once
#include "MUID.h"
#include "imgui.h"


namespace MMMEngine::Editor
{
	std::string GetString(const char* type)
	{
		std::string result;

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type))
			{
				if (payload->IsDelivery() && payload->Data && payload->DataSize > 0)
				{
					const char* cstr = static_cast<const char*>(payload->Data);
					result.assign(cstr);
				}
			}
			ImGui::EndDragDropTarget();
		}
		return result;
	}

	Utility::MUID GetMuid(const std::string& type)
	{
		Utility::MUID result = Utility::MUID::Empty();

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type.c_str()))
			{
				if (payload->IsDelivery() && payload->Data && payload->DataSize == sizeof(Utility::MUID))
				{
					std::memcpy(&result, payload->Data, sizeof(Utility::MUID));
				}
			}
			ImGui::EndDragDropTarget();
		}
		return result;
	}

	void GiveString(const char* type, const std::string& info, const std::string& display)
	{
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(type, info.c_str(), (int)info.size() + 1); // '\0' Æ÷ÇÔ
			ImGui::TextUnformatted(display.c_str());
			ImGui::EndDragDropSource();
		}
	}

	void GiveMuid(std::string type, Utility::MUID muid, std::string display)
	{
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(type.c_str(), &muid, sizeof(Utility::MUID));
			ImGui::Text(display.c_str());
			ImGui::EndDragDropSource();
		}
	}
}
