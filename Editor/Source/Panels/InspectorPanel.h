#pragma once

#include "Panels/EditorPanel.h"
#include "Panels/Inspector/AssetInspector.h"
#include "Panels/Inspector/ObjectInspector.h"

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class InspectorPanel : public EditorPanel
    {
    public:
        InspectorPanel(EditorApplication& application, int instanceIndex);
        ~InspectorPanel();

        void Draw() override;

    private:
        void DrawSelectedObjects();

        EditorApplication& m_application;
        ObjectInspector m_object;
        AssetInspector m_asset;
    };
}
