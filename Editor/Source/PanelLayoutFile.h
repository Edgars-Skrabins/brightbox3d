#pragma once

#include "EditorWindows.h"
#include "Panels/EditorPanel.h"
#include "Serialization/Json.h"

#include <memory>
#include <optional>
#include <vector>

namespace BrightBox3D::Editor::PanelLayoutFile
{
    inline constexpr const char* FileName = "EditorPanels.json";

    struct PanelEntry
    {
        EditorWindow type = EditorWindow::LevelView;
        int index = 1;
        const Serialization::JsonValue* state = nullptr;
    };

    bool IsDockablePanel(EditorWindow type);
    void Write(const std::vector<std::unique_ptr<EditorPanel>>& panels, Serialization::JsonDocument& document);
    std::optional<std::vector<PanelEntry>> Read(const Serialization::JsonValue& document);
}
