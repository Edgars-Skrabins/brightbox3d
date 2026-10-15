#pragma once

#include "Core/Guid.h"
#include "Panels/EditorPanel.h"
#include "Panels/Inspector/AssetInspector.h"
#include "Panels/Inspector/ObjectInspector.h"
#include "Services/SelectionService.h"

#include <optional>

namespace BrightBox3D::Editor
{
    class EditorApplication;

    struct InspectorTarget
    {
        SelectionFocus focus = SelectionFocus::None;
        Core::Guid object;
        Core::Guid asset;
        Level::LevelCollection* collection = nullptr;
    };

    class InspectorPanel : public EditorPanel
    {
    public:
        InspectorPanel(EditorApplication& application, int instanceIndex);
        ~InspectorPanel();

        void Draw() override;
        bool IsLocked() const { return m_locked.has_value(); }
        void SetLocked(bool locked);
        void SaveState(Serialization::JsonValue& state, Serialization::JsonAllocator& allocator) const override;
        void LoadState(const Serialization::JsonValue& state) override;

    private:
        InspectorTarget SelectedTarget() const;
        bool TargetExists(const InspectorTarget& target) const;
        void DrawLockButton();
        void DrawSelectedObjects();
        void DrawTarget(const InspectorTarget& target);
        void DrawMissingTarget();

        EditorApplication& m_application;
        ObjectInspector m_object;
        AssetInspector m_asset;
        std::optional<InspectorTarget> m_locked;
        bool m_lockPending = false;
    };
}
