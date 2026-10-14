#pragma once

#include "Core/Guid.h"
#include "Panels/Inspector/ImportSettingsInspector.h"
#include "Panels/Inspector/MaterialInspector.h"

namespace BrightBox3D::Assets
{
    struct AssetRecord;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class AssetInspector
    {
    public:
        explicit AssetInspector(EditorApplication& application);

        void Draw(const Core::Guid& guid);

    private:
        void DrawHeader(const Assets::AssetRecord& record);
        void DrawLevel(const Core::Guid& guid);
        void DrawVariant(const Core::Guid& guid);
        void DrawTemplate(const Core::Guid& guid);
        void DrawTexture(const Core::Guid& guid);
        void DrawModel(const Assets::AssetRecord& record);
        void DrawTextAsset(const Assets::AssetRecord& record);

        EditorApplication& m_application;
        MaterialInspector m_material;
        ImportSettingsInspector m_importSettings;
    };
}
