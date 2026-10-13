#pragma once

#include "Core/Guid.h"
#include "Level/Level.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorContext;

    class AssetCreationService
    {
    public:
        explicit AssetCreationService(EditorContext& context) : m_context(context) {}

        Core::Guid CreateVariantAsset(Level::LevelObject& object, const std::string& folder);
        Core::Guid CreateTemplateAsset(Level::LevelObject* object, const std::string& folder);
        Core::Guid CreateVariantFromTemplate(const Core::Guid& templateAsset);

        const std::string& GetAssetsFolder() const { return m_assetsFolder; }
        void SetAssetsFolder(const std::string& folder) { m_assetsFolder = folder; }

    private:
        EditorContext& m_context;
        std::string m_assetsFolder;
    };
}
