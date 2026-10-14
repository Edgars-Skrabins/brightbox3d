#pragma once

#include "Panels/Inspector/AssetInspector.h"
#include "Panels/Inspector/ObjectInspector.h"

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class InspectorPanel
    {
    public:
        explicit InspectorPanel(EditorApplication& application);
        ~InspectorPanel();

        void Draw();

    private:
        void DrawSelectedObjects();

        EditorApplication& m_application;
        ObjectInspector m_object;
        AssetInspector m_asset;
    };
}
