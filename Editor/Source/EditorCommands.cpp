#include "EditorCommands.h"

#include "Commands/CommandRegistry.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "HotReload.h"
#include "Panels/LevelContentsPanel.h"
#include "Panels/LevelViewPanel.h"

namespace BrightBox3D::Editor
{
    namespace
    {
        KeyChord Chord(ImGuiKey key, ImGuiKeyChord modifiers = 0)
        {
            return { key, modifiers };
        }

        void RegisterFileCommands(CommandRegistry& registry, EditorApplication& application)
        {
            EditorContext& context = application.GetContext();
            auto notPlaying = [&context] { return !context.GetLevelService().IsPlaying(); };
            registry.Register(CommandId::NewLevel, {
                .key = "NewLevel",
                .icon = Icon::FilePlus,
                .label = EditorText::NewLevelMenu,
                .tooltip = EditorText::NewLevelTooltip,
                .isEnabled = notPlaying,
                .execute = [&application] { application.OpenNewLevelDialog(); },
            });
            registry.Register(CommandId::Save, {
                .key = "Save",
                .icon = Icon::Save,
                .label = EditorText::Save,
                .tooltip = EditorText::SaveTooltip,
                .defaultShortcut = Chord(ImGuiKey_S, ImGuiMod_Ctrl),
                .isEnabled = notPlaying,
                .execute = [&context] { context.GetLevelService().SaveLevel(); },
            });
            registry.Register(CommandId::BuildGame, {
                .key = "BuildGame",
                .icon = Icon::Package,
                .label = EditorText::BuildGame,
                .tooltip = EditorText::BuildGameTooltip,
                .isEnabled = [&context, &application] { return !context.GetLevelService().IsPlaying() && !application.GetHotReload()->IsCompiling(); },
                .execute = [&application] { application.BuildGame(); },
            });
            registry.Register(CommandId::ProjectSettings, {
                .key = "ProjectSettings",
                .icon = Icon::SlidersHorizontal,
                .label = EditorText::ProjectSettingsMenu,
                .execute = [&application] { application.OpenProjectSettings(); },
            });
            registry.Register(CommandId::OpenOtherProject, {
                .key = "OpenOtherProject",
                .icon = Icon::FolderOpen,
                .label = EditorText::OpenOtherProject,
                .execute = [&application] { application.CloseProject(); },
            });
            registry.Register(CommandId::Exit, {
                .key = "Exit",
                .icon = Icon::LogOut,
                .label = EditorText::Exit,
                .execute = [&application] { application.RequestExit(); },
            });
        }

        void RegisterEditCommands(CommandRegistry& registry, EditorApplication& application)
        {
            EditorContext& context = application.GetContext();
            auto hasSelection = [&context] { return !context.GetSelectionService().GetSelection().empty(); };
            registry.Register(CommandId::Undo, {
                .key = "Undo",
                .icon = Icon::Undo2,
                .label = EditorText::Undo,
                .defaultShortcut = Chord(ImGuiKey_Z, ImGuiMod_Ctrl),
                .isEnabled = [&context] { return context.GetUndoService().GetUndo().CanUndo() && !context.GetLevelService().IsPlaying(); },
                .execute = [&context] { context.GetUndoService().Undo(); },
                .dynamicLabel = [&context] { return Text(EditorText::UndoMenuItem, context.GetUndoService().GetUndo().GetUndoName()); },
            });
            registry.Register(CommandId::Redo, {
                .key = "Redo",
                .icon = Icon::Redo2,
                .label = EditorText::Redo,
                .defaultShortcut = Chord(ImGuiKey_Z, ImGuiMod_Ctrl | ImGuiMod_Shift),
                .isEnabled = [&context] { return context.GetUndoService().GetUndo().CanRedo() && !context.GetLevelService().IsPlaying(); },
                .execute = [&context] { context.GetUndoService().Redo(); },
                .dynamicLabel = [&context] { return Text(EditorText::RedoMenuItem, context.GetUndoService().GetUndo().GetRedoName()); },
            });
            registry.Register(CommandId::Duplicate, {
                .key = "Duplicate",
                .icon = Icon::Copy,
                .label = EditorText::Duplicate,
                .tooltip = EditorText::DuplicateTooltip,
                .defaultShortcut = Chord(ImGuiKey_D, ImGuiMod_Ctrl),
                .isEnabled = hasSelection,
                .execute = [&context] { context.GetObjectEditService().DuplicateSelection(); },
            });
            registry.Register(CommandId::Delete, {
                .key = "Delete",
                .icon = Icon::Trash2,
                .label = EditorText::Delete,
                .tooltip = EditorText::DeleteTooltip,
                .defaultShortcut = Chord(ImGuiKey_Delete),
                .isEnabled = hasSelection,
                .execute = [&context] { context.GetObjectEditService().DeleteSelection(); },
            });
            registry.Register(CommandId::Preferences, {
                .key = "Preferences",
                .icon = Icon::Settings,
                .label = EditorText::PreferencesMenu,
                .defaultShortcut = Chord(ImGuiKey_Comma, ImGuiMod_Ctrl),
                .execute = [&application] { application.OpenPreferences(); },
            });
            registry.Register(CommandId::ResetLayout, {
                .key = "ResetLayout",
                .icon = Icon::LayoutDashboard,
                .label = EditorText::ResetLayout,
                .execute = [&application] { application.ResetLayout(); },
            });
        }

        void RegisterGameCommands(CommandRegistry& registry, EditorApplication& application)
        {
            EditorContext& context = application.GetContext();
            auto playing = [&context] { return context.GetLevelService().IsPlaying(); };
            auto compiling = [&application] { return application.GetHotReload()->IsCompiling(); };
            registry.Register(CommandId::Play, {
                .key = "Play",
                .icon = Icon::Play,
                .label = EditorText::Play,
                .tooltip = EditorText::PlayTooltip,
                .defaultShortcut = Chord(ImGuiKey_P, ImGuiMod_Ctrl),
                .isChecked = playing,
                .execute = [&application, playing] { playing() ? application.RequestStop() : application.RequestPlay(); },
                .dynamicLabel = [playing] { return std::string(Text(playing() ? EditorText::Stop : EditorText::Play)); },
                .dynamicIcon = [playing] { return playing() ? Icon::Square : Icon::Play; },
                .checkedColor = EditorColor::PlayMode,
            });
            registry.Register(CommandId::Pause, {
                .key = "Pause",
                .icon = Icon::Pause,
                .label = EditorText::Pause,
                .tooltip = EditorText::PauseTooltip,
                .isEnabled = playing,
                .execute = [&application] { application.GetPlaySession()->SetPaused(!application.GetPlaySession()->IsPaused()); },
                .dynamicLabel = [&application] { return std::string(Text(application.GetPlaySession()->IsPaused() ? EditorText::Resume : EditorText::Pause)); },
                .dynamicIcon = [&application] { return application.GetPlaySession()->IsPaused() ? Icon::Play : Icon::Pause; },
            });
            registry.Register(CommandId::Step, {
                .key = "Step",
                .icon = Icon::StepForward,
                .label = EditorText::Step,
                .tooltip = EditorText::StepTooltip,
                .isEnabled = playing,
                .execute = [&application]
                {
                    application.GetPlaySession()->SetPaused(true);
                    application.GetPlaySession()->Step();
                },
            });
            registry.Register(CommandId::Compile, {
                .key = "Compile",
                .icon = Icon::Hammer,
                .label = EditorText::Compile,
                .tooltip = EditorText::CompileTooltip,
                .defaultShortcut = Chord(ImGuiKey_B, ImGuiMod_Ctrl),
                .isEnabled = [compiling] { return !compiling(); },
                .execute = [&application] { application.GetHotReload()->RequestCompile(); },
            });
            registry.Register(CommandId::CancelCompile, {
                .key = "CancelCompile",
                .icon = Icon::X,
                .label = EditorText::CancelCompile,
                .tooltip = EditorText::CancelCompileTooltip,
                .isEnabled = compiling,
                .execute = [&application] { application.GetHotReload()->Cancel(); },
            });
        }

        void RegisterGizmoOperation(CommandRegistry& registry, EditorApplication& application, CommandId id, const char* key, Icon icon, EditorText label, EditorText tooltip, ImGuiKey shortcut, GizmoOperation operation)
        {
            registry.Register(id, {
                .key = key,
                .icon = icon,
                .label = label,
                .tooltip = tooltip,
                .scope = CommandScope::LevelView,
                .defaultShortcut = Chord(shortcut),
                .isChecked = [&application, operation] { return application.GetActiveLevelView().GetGizmoOperation() == operation; },
                .execute = [&application, operation] { application.GetActiveLevelView().SetGizmoOperation(operation); },
            });
        }

        void RegisterLevelViewCommands(CommandRegistry& registry, EditorApplication& application)
        {
            RegisterGizmoOperation(registry, application, CommandId::GizmoMove, "GizmoMove", Icon::Move, EditorText::GizmoMove, EditorText::GizmoMoveTooltip, ImGuiKey_W, GizmoOperation::Translate);
            RegisterGizmoOperation(registry, application, CommandId::GizmoRotate, "GizmoRotate", Icon::RotateCw, EditorText::GizmoRotate, EditorText::GizmoRotateTooltip, ImGuiKey_E, GizmoOperation::Rotate);
            RegisterGizmoOperation(registry, application, CommandId::GizmoScale, "GizmoScale", Icon::Scaling, EditorText::GizmoScale, EditorText::GizmoScaleTooltip, ImGuiKey_R, GizmoOperation::Scale);
            registry.Register(CommandId::GizmoSpace, {
                .key = "GizmoSpace",
                .icon = Icon::Globe,
                .label = EditorText::GizmoWorld,
                .tooltip = EditorText::GizmoSpaceTooltip,
                .scope = CommandScope::LevelView,
                .execute = [&application] { application.GetActiveLevelView().SetGizmoLocal(!application.GetActiveLevelView().IsGizmoLocal()); },
                .dynamicLabel = [&application] { return std::string(Text(application.GetActiveLevelView().IsGizmoLocal() ? EditorText::GizmoLocal : EditorText::GizmoWorld)); },
            });
            registry.Register(CommandId::Snap, {
                .key = "Snap",
                .icon = Icon::Magnet,
                .label = EditorText::Snap,
                .tooltip = EditorText::SnapTooltip,
                .scope = CommandScope::LevelView,
                .isChecked = [&application] { return application.GetActiveLevelView().IsSnapping(); },
                .execute = [&application] { application.GetActiveLevelView().SetSnapping(!application.GetActiveLevelView().IsSnapping()); },
            });
            registry.Register(CommandId::Grid, {
                .key = "Grid",
                .icon = Icon::Grid,
                .label = EditorText::Grid,
                .tooltip = EditorText::GridTooltip,
                .scope = CommandScope::LevelView,
                .isChecked = [&application] { return application.GetActiveLevelView().IsGridVisible(); },
                .execute = [&application] { application.GetActiveLevelView().SetGridVisible(!application.GetActiveLevelView().IsGridVisible()); },
            });
            registry.Register(CommandId::FocusSelection, {
                .key = "FocusSelection",
                .icon = Icon::Focus,
                .label = EditorText::Focus,
                .tooltip = EditorText::FocusTooltip,
                .scope = CommandScope::LevelView,
                .defaultShortcut = Chord(ImGuiKey_F),
                .execute = [&application] { application.GetActiveLevelView().FocusSelection(); },
            });
        }

        void RegisterLevelContentsCommands(CommandRegistry& registry, EditorApplication& application)
        {
            registry.Register(CommandId::Rename, {
                .key = "Rename",
                .icon = Icon::Pencil,
                .label = EditorText::Rename,
                .tooltip = EditorText::RenameTooltip,
                .scope = CommandScope::LevelContents,
                .defaultShortcut = Chord(ImGuiKey_F2),
                .isEnabled = [&application] { return application.GetContext().GetSelectionService().GetPrimarySelection() != nullptr; },
                .execute = [&application] { application.GetLevelContents().BeginRenameSelection(); },
            });
        }
    }

    void RegisterEditorCommands(EditorApplication& application)
    {
        CommandRegistry& registry = CommandRegistry::Get();
        RegisterFileCommands(registry, application);
        RegisterEditCommands(registry, application);
        RegisterGameCommands(registry, application);
        RegisterLevelViewCommands(registry, application);
        RegisterLevelContentsCommands(registry, application);
    }
}
