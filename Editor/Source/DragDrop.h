#pragma once

#include "Core/Guid.h"

#include <optional>

namespace BrightBox3D::Editor
{
    enum class DragDropPayload
    {
        Asset,
        Object,
        Script,
        ScriptFolder
    };

    void SetPayload(const Core::Guid& guid, DragDropPayload payload);
    std::optional<Core::Guid> AcceptPayload(DragDropPayload payload);
}
