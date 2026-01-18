#pragma once
#include "MUID.h"
#include "imgui.h"


namespace MMMEngine::Editor
{
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
