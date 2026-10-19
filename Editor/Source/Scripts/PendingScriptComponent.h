#pragma once

#include "Core/Guid.h"

#include <optional>
#include <string>

namespace BrightBox3D::Editor
{
    class EditorContext;

    class PendingScriptComponent
    {
    public:
        void Request(const Core::Guid& object, const std::string& className);
        void Clear() { m_request.reset(); }
        bool IsPending() const { return m_request.has_value(); }
        void OnReloaded(EditorContext& context);

    private:
        struct Target
        {
            Core::Guid object;
            std::string className;
        };

        std::optional<Target> m_request;
    };
}
