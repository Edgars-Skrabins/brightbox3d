#include "DragDrop.h"

#include <imgui.h>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr const char* PayloadIds[] = { "BRIGHT_ASSET", "BRIGHT_OBJECT", "BRIGHT_SCRIPT", "BRIGHT_SCRIPT_FOLDER" };

        const char* PayloadId(DragDropPayload payload)
        {
            return PayloadIds[static_cast<size_t>(payload)];
        }
    }

    void SetPayload(const Core::Guid& guid, DragDropPayload payload)
    {
        ImGui::SetDragDropPayload(PayloadId(payload), &guid, sizeof(guid));
    }

    std::optional<Core::Guid> AcceptPayload(DragDropPayload payload)
    {
        if (const ImGuiPayload* accepted = ImGui::AcceptDragDropPayload(PayloadId(payload)))
        {
            return *static_cast<const Core::Guid*>(accepted->Data);
        }
        return std::nullopt;
    }
}
