#include "EditorCommands.h"

#include "Assets/AssetDatabase.h"
#include "Commands/CommandRegistry.h"
#include "EditorApplication.h"
#include "EditorPlaySession.h"
#include "HotReload.h"
#include "Panels/Assets/AssetActions.h"
#include "Panels/Assets/ScriptActions.h"
#include "Panels/AssetsPanel.h"
#include "Panels/LevelContentsPanel.h"
#include "Panels/LevelViewPanel.h"

namespace BrightBox3D::Editor
{
    namespace
    {
#if defined(__APPLE__)
        constexpr EditorText ShowInFileBrowserLabel = EditorText::ShowInFinder;
#elif defined(_WIN32)
        constexpr EditorText ShowInFileBrowserLabel = EditorText::ShowInExplorer;
#else
        constexpr EditorText ShowInFileBrowserLabel = EditorText::RevealInFileBrowser;
#endif

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
            registry.Register(CommandId::NewScript, {
                .key = "NewScript",
                .icon = Icon::FileCode,
                .label = EditorText::NewScriptMenu,
                .tooltip = EditorText::NewScriptTooltip,
                .defaultShortcut = Chord(ImGuiKey_N, ImGuiMod_Ctrl | ImGuiMod_Shift),
                .isEnabled = [&context, notPlaying] { return notPlaying() && context.GetProjectService().HasProject(); },
                .execute = [&application] { application.OpenCreateScriptDialog(application.DefaultScriptRequest()); },
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
                .isEnabled = [&application] { return application.GetActiveLevelView() != nullptr; },
                .isChecked = [&application, operation]
                {
                    LevelViewPanel* view = application.GetActiveLevelView();
                    return view && view->GetGizmoOperation() == operation;
                },
                .execute = [&application, operation] { application.GetActiveLevelView()->SetGizmoOperation(operation); },
            });
        }

        void RegisterLevelViewCommands(CommandRegistry& registry, EditorApplication& application)
        {
            auto hasView = [&application] { return application.GetActiveLevelView() != nullptr; };
            RegisterGizmoOperation(registry, application, CommandId::GizmoMove, "GizmoMove", Icon::Move, EditorText::GizmoMove, EditorText::GizmoMoveTooltip, ImGuiKey_W, GizmoOperation::Translate);
            RegisterGizmoOperation(registry, application, CommandId::GizmoRotate, "GizmoRotate", Icon::RotateCw, EditorText::GizmoRotate, EditorText::GizmoRotateTooltip, ImGuiKey_E, GizmoOperation::Rotate);
            RegisterGizmoOperation(registry, application, CommandId::GizmoScale, "GizmoScale", Icon::Scaling, EditorText::GizmoScale, EditorText::GizmoScaleTooltip, ImGuiKey_R, GizmoOperation::Scale);
            registry.Register(CommandId::GizmoSpace, {
                .key = "GizmoSpace",
                .icon = Icon::Globe,
                .label = EditorText::GizmoWorld,
                .tooltip = EditorText::GizmoSpaceTooltip,
                .scope = CommandScope::LevelView,
                .isEnabled = hasView,
                .execute = [&application]
                {
                    LevelViewPanel* view = application.GetActiveLevelView();
                    view->SetGizmoLocal(!view->IsGizmoLocal());
                },
                .dynamicLabel = [&application]
                {
                    LevelViewPanel* view = application.GetActiveLevelView();
                    return std::string(Text(view && view->IsGizmoLocal() ? EditorText::GizmoLocal : EditorText::GizmoWorld));
                },
            });
            registry.Register(CommandId::Snap, {
                .key = "Snap",
                .icon = Icon::Magnet,
                .label = EditorText::Snap,
                .tooltip = EditorText::SnapTooltip,
                .scope = CommandScope::LevelView,
                .isEnabled = hasView,
                .isChecked = [&application]
                {
                    LevelViewPanel* view = application.GetActiveLevelView();
                    return view && view->IsSnapping();
                },
                .execute = [&application]
                {
                    LevelViewPanel* view = application.GetActiveLevelView();
                    view->SetSnapping(!view->IsSnapping());
                },
            });
            registry.Register(CommandId::Grid, {
                .key = "Grid",
                .icon = Icon::Grid,
                .label = EditorText::Grid,
                .tooltip = EditorText::GridTooltip,
                .scope = CommandScope::LevelView,
                .isEnabled = hasView,
                .isChecked = [&application]
                {
                    LevelViewPanel* view = application.GetActiveLevelView();
                    return view && view->IsGridVisible();
                },
                .execute = [&application]
                {
                    LevelViewPanel* view = application.GetActiveLevelView();
                    view->SetGridVisible(!view->IsGridVisible());
                },
            });
            registry.Register(CommandId::FocusSelection, {
                .key = "FocusSelection",
                .icon = Icon::Focus,
                .label = EditorText::Focus,
                .tooltip = EditorText::FocusTooltip,
                .scope = CommandScope::LevelView,
                .defaultShortcut = Chord(ImGuiKey_F),
                .isEnabled = hasView,
                .execute = [&application] { application.GetActiveLevelView()->FocusSelection(); },
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
                .isEnabled = [&application] { return application.GetLevelContents() && application.GetContext().GetSelectionService().GetPrimarySelection() != nullptr; },
                .execute = [&application] { application.GetLevelContents()->BeginRenameSelection(); },
            });
        }

        void RegisterNewPanel(CommandRegistry& registry, EditorApplication& application, CommandId id, const char* key, EditorText label, EditorWindow type)
        {
            registry.Register(id, {
                .key = key,
                .icon = GetWindowInfo(type).icon,
                .label = label,
                .tooltip = EditorText::FloatingPanelHint,
                .execute = [&application, type] { application.CreatePanel(type); },
            });
        }

        void RegisterWindowCommands(CommandRegistry& registry, EditorApplication& application)
        {
            RegisterNewPanel(registry, application, CommandId::NewLevelView, "NewLevelView", EditorText::NewLevelViewPanel, EditorWindow::LevelView);
            RegisterNewPanel(registry, application, CommandId::NewLevelContents, "NewLevelContents", EditorText::NewLevelContentsPanel, EditorWindow::LevelContents);
            RegisterNewPanel(registry, application, CommandId::NewInspector, "NewInspector", EditorText::NewInspectorPanel, EditorWindow::Inspector);
            RegisterNewPanel(registry, application, CommandId::NewAssets, "NewAssets", EditorText::NewAssetsPanel, EditorWindow::Assets);
            RegisterNewPanel(registry, application, CommandId::NewTerminal, "NewTerminal", EditorText::NewTerminalPanel, EditorWindow::Terminal);
        }

        void RegisterAssetsLayout(CommandRegistry& registry, CommandId id, const char* key, Icon icon, EditorText label, AssetsLayout layout)
        {
            registry.Register(id, {
                .key = key,
                .icon = icon,
                .label = label,
                .scope = CommandScope::Assets,
                .isChecked = [layout] { return EditorPreferences::Get().GetAssetsLayout() == layout; },
                .execute = [layout]
                {
                    EditorPreferences::Get().SetAssetsLayout(layout);
                    EditorPreferences::Get().Save();
                },
            });
        }

        void RegisterAssetsCommands(CommandRegistry& registry, EditorApplication& application)
        {
            auto target = [&application]
            {
                AssetsPanel* assets = application.GetAssets();
                return assets ? assets->GetCommandTarget() : AssetsTarget{};
            };
            auto editable = [target] { return AssetActions::IsEditable(target()); };
            auto inScripts = [target] { return target().root == AssetsRoot::Scripts; };
            auto scriptItem = [target]
            {
                AssetsTarget current = target();
                return current.root == AssetsRoot::Scripts && (current.script.IsValid() || !current.folder.empty());
            };
            registry.Register(CommandId::AssetShowInFileBrowser, {
                .key = "AssetShowInFileBrowser",
                .icon = Icon::FolderSearch,
                .label = ShowInFileBrowserLabel,
                .scope = CommandScope::Assets,
                .isEnabled = [target]
                {
                    AssetsTarget current = target();
                    if (current.root != AssetsRoot::Assets)
                    {
                        return current.root == AssetsRoot::Scripts;
                    }
                    return current.asset.IsValid() ? AssetActions::IsEditable(current) : AssetActions::CanCreateIn(current.folder);
                },
                .execute = [&application, target, inScripts]
                {
                    if (inScripts())
                    {
                        ScriptActions::ShowInFileBrowser(application, target());
                        return;
                    }
                    AssetActions::ShowInFileBrowser(application, target());
                },
            });
            registry.Register(CommandId::AssetOpen, {
                .key = "AssetOpen",
                .icon = Icon::ExternalLink,
                .label = EditorText::Open,
                .scope = CommandScope::Assets,
                .isEnabled = [editable, scriptItem] { return editable() || scriptItem(); },
                .execute = [&application, target, inScripts]
                {
                    if (inScripts())
                    {
                        ScriptActions::Open(application, application.GetAssets()->GetState(), target());
                        return;
                    }
                    AssetActions::Open(application, application.GetAssets()->GetState(), target().asset);
                },
            });
            registry.Register(CommandId::ScriptOpenHeader, {
                .key = "ScriptOpenHeader",
                .icon = Icon::FileCode,
                .label = EditorText::OpenHeader,
                .scope = CommandScope::Assets,
                .isEnabled = [&application, target] { return ScriptActions::HasHeader(application, target()); },
                .execute = [&application, target] { ScriptActions::OpenHeader(application, target()); },
            });
            registry.Register(CommandId::AssetRename, {
                .key = "AssetRename",
                .icon = Icon::Pencil,
                .label = EditorText::Rename,
                .scope = CommandScope::Assets,
                .defaultShortcut = Chord(ImGuiKey_F2),
                .isEnabled = [editable, scriptItem] { return editable() || scriptItem(); },
                .execute = [&application, target, inScripts]
                {
                    if (inScripts())
                    {
                        ScriptActions::BeginRename(application, application.GetAssets()->GetState(), target());
                        return;
                    }
                    AssetActions::BeginRename(application.GetAssets()->GetState(), target());
                },
            });
            registry.Register(CommandId::AssetDelete, {
                .key = "AssetDelete",
                .icon = Icon::Trash2,
                .label = EditorText::Delete,
                .scope = CommandScope::Assets,
                .defaultShortcut = Chord(ImGuiKey_Delete),
                .isEnabled = editable,
                .execute = [&application, target] { application.GetAssets()->GetState().pendingDelete = target().asset; },
            });
            registry.Register(CommandId::AssetDuplicate, {
                .key = "AssetDuplicate",
                .icon = Icon::Copy,
                .label = EditorText::Duplicate,
                .scope = CommandScope::Assets,
                .defaultShortcut = Chord(ImGuiKey_D, ImGuiMod_Ctrl),
                .isEnabled = editable,
                .execute = [&application, target] { AssetActions::Duplicate(application, target()); },
            });
            registry.Register(CommandId::AssetCopyPath, {
                .key = "AssetCopyPath",
                .icon = Icon::ClipboardCopy,
                .label = EditorText::CopyPath,
                .scope = CommandScope::Assets,
                .defaultShortcut = Chord(ImGuiKey_C, ImGuiMod_Ctrl | ImGuiMod_Alt),
                .isEnabled = [target] { return target().root != AssetsRoot::EngineClasses; },
                .execute = [target, inScripts]
                {
                    if (inScripts())
                    {
                        ScriptActions::CopyPath(target());
                        return;
                    }
                    AssetActions::CopyPath(target());
                },
            });
            registry.Register(CommandId::AssetSetStartupLevel, {
                .key = "AssetSetStartupLevel",
                .icon = Icon::Flag,
                .label = EditorText::SetStartupLevel,
                .scope = CommandScope::Assets,
                .isEnabled = [target] { return AssetActions::IsAssetOfType(target(), Assets::AssetType::Level); },
                .execute = [&application, target] { AssetActions::SetStartupLevel(application, target()); },
            });
            registry.Register(CommandId::AssetCreateVariant, {
                .key = "AssetCreateVariant",
                .icon = Icon::Package,
                .label = EditorText::CreateVariant,
                .scope = CommandScope::Assets,
                .isEnabled = [target] { return AssetActions::IsAssetOfType(target(), Assets::AssetType::LevelObjectTemplate); },
                .execute = [&application, target] { AssetActions::CreateVariant(application, target()); },
            });
            registry.Register(CommandId::AssetReimport, {
                .key = "AssetReimport",
                .icon = Icon::RefreshCcw,
                .label = EditorText::Reimport,
                .scope = CommandScope::Assets,
                .isEnabled = [target, editable] { return editable() && !AssetActions::IsAssetOfType(target(), Assets::AssetType::Folder); },
                .execute = [target] { AssetActions::Reimport(target()); },
            });
            registry.Register(CommandId::AssetRefresh, {
                .key = "AssetRefresh",
                .icon = Icon::RefreshCw,
                .label = EditorText::Refresh,
                .scope = CommandScope::Assets,
                .defaultShortcut = Chord(ImGuiKey_R, ImGuiMod_Ctrl),
                .execute = [] { Assets::AssetDatabase::Get().Refresh(); },
            });
            RegisterAssetsLayout(registry, CommandId::AssetsOneColumnLayout, "AssetsOneColumnLayout", Icon::ListTree, EditorText::OneColumnLayout, AssetsLayout::OneColumn);
            RegisterAssetsLayout(registry, CommandId::AssetsTwoColumnLayout, "AssetsTwoColumnLayout", Icon::Columns2, EditorText::TwoColumnLayout, AssetsLayout::TwoColumn);
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
        RegisterAssetsCommands(registry, application);
        RegisterWindowCommands(registry, application);
    }
}
