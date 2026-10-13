#pragma once

#include "Commands/EditorCommand.h"

#include <optional>
#include <vector>

namespace BrightBox3D::Editor
{
    class PreferencesPanel
    {
    public:
        void Draw();
        void Open() { m_open = true; }

    private:
        struct PendingShortcut
        {
            CommandId id;
            KeyChord chord;
            std::vector<CommandId> conflicts;
        };

        void DrawColorsPage();
        void DrawColorRow(EditorColor color);
        void DrawShortcutsPage();
        void DrawShortcutRow(CommandId id);
        void CaptureShortcut();
        void AssignShortcut(CommandId id, const KeyChord& chord);
        void DrawMoveShortcutPopup();

        bool m_open = false;
        std::optional<CommandId> m_capturing;
        std::optional<PendingShortcut> m_pending;
        bool m_openMovePopup = false;
    };
}
