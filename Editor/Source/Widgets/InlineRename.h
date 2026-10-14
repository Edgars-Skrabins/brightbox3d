#pragma once

#include "Core/Guid.h"

#include <optional>
#include <string>

namespace BrightBox3D::Editor
{
    class InlineRename
    {
    public:
        void Begin(const Core::Guid& target, const std::string& text);
        bool IsRenaming(const Core::Guid& target) const { return m_target == target; }
        std::optional<std::string> Draw(float width);

    private:
        Core::Guid m_target;
        std::string m_buffer;
        bool m_focus = false;
    };
}
