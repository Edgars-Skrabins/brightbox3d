#pragma once

#include "Core/Guid.h"
#include "Reflection/EngineObject.h"

#include <memory>

namespace BrightBox3D::Editor
{
    class ImportSettingsInspector
    {
    public:
        void Draw(const Core::Guid& guid);

    private:
        Core::Guid m_asset;
        std::unique_ptr<Reflection::EngineObject> m_settings;
        bool m_dirty = false;
    };
}
