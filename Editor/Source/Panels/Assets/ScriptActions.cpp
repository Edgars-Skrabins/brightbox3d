#include "Panels/Assets/ScriptActions.h"

#include "Core/Log.h"
#include "DragDrop.h"
#include "EditorApplication.h"
#include "EditorText.h"
#include "ObjectIcons.h"
#include "Reflection/TypeRegistry.h"
#include "Runtime/ProjectLayout.h"
#include "Scripts/ScriptFiles.h"
#include "Services/ScriptLibrary.h"

#include <imgui.h>

namespace BrightBox3D::Editor::ScriptActions
{
    namespace
    {
        ScriptLibrary& Library(EditorApplication& application)
        {
            return application.GetContext().GetScriptLibrary();
        }

        void Report(const EditorError& error)
        {
            Core::Log::Error(error.GetText());
        }
    }

    AssetsTarget TargetForScript(const ScriptEntry& script)
    {
        return { {}, script.folder, AssetsRoot::Scripts, script.id };
    }

    AssetsTarget TargetForFolder(const std::string& folder)
    {
        return { {}, folder, AssetsRoot::Scripts, {} };
    }

    Icon ScriptIcon(const ScriptEntry& script)
    {
        const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(script.className);
        Icon icon = type ? ObjectIcons::TypeIcon(*type) : Icon::None;
        return icon == Icon::None ? Icon::FileCode : icon;
    }

    std::string ScriptKind(const ScriptEntry& script)
    {
        const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(script.className);
        return type && type->GetBase() ? type->GetBase()->GetName() : std::string();
    }

    std::string DisplayPath(const AssetsTarget& target)
    {
        std::string root = Runtime::ProjectLayout::ScriptsFolder;
        return target.folder.empty() ? root : root + "/" + target.folder;
    }

    void Open(EditorApplication& application, AssetsPanelState& state, const AssetsTarget& target)
    {
        if (const ScriptEntry* script = Library(application).Find(target.script))
        {
            application.OpenFileInEditor(script->PrimaryFile().string(), 0);
            return;
        }
        state.root = AssetsRoot::Scripts;
        state.scriptFolder = target.folder;
    }

    bool HasHeader(EditorApplication& application, const AssetsTarget& target)
    {
        const ScriptEntry* script = Library(application).Find(target.script);
        return script && !script->header.empty();
    }

    void OpenHeader(EditorApplication& application, const AssetsTarget& target)
    {
        if (const ScriptEntry* script = Library(application).Find(target.script); script && !script->header.empty())
        {
            application.OpenFileInEditor(script->header.string(), 0);
        }
    }

    void ShowInFileBrowser(EditorApplication& application, const AssetsTarget& target)
    {
        ScriptLibrary& library = Library(application);
        if (const ScriptEntry* script = library.Find(target.script))
        {
            application.RevealInFileBrowser(script->PrimaryFile());
            return;
        }
        application.RevealInFileBrowser(library.FolderPath(target.folder));
    }

    void CopyPath(const AssetsTarget& target)
    {
        ImGui::SetClipboardText(DisplayPath(target).c_str());
    }

    void CreateFolder(EditorApplication& application, AssetsPanelState& state, const std::string& parent)
    {
        ScriptLibrary& library = Library(application);
        Core::Result<std::string, EditorError> folder = ScriptFiles::CreateFolder(library.GetRoot(), parent);
        if (!folder)
        {
            Report(folder.GetError());
            return;
        }
        library.Refresh();
        state.rename.Begin(ScriptLibrary::FolderId(*folder), ScriptFiles::FolderName(*folder));
    }

    void BeginRename(EditorApplication& application, AssetsPanelState& state, const AssetsTarget& target)
    {
        if (!target.script.IsValid() && !target.folder.empty())
        {
            state.rename.Begin(ScriptLibrary::FolderId(target.folder), ScriptFiles::FolderName(target.folder));
        }
    }

    void CommitFolderRename(EditorApplication& application, AssetsPanelState& state, const std::string& folder, const std::string& name)
    {
        ScriptLibrary& library = Library(application);
        Core::Result<std::string, EditorError> renamed = ScriptFiles::RenameFolder(library.GetRoot(), folder, name);
        if (!renamed)
        {
            Report(renamed.GetError());
            return;
        }
        if (state.scriptFolder == folder)
        {
            state.scriptFolder = *renamed;
        }
        library.Refresh();
    }

    void BeginDrag(const ScriptEntry& script)
    {
        if (ImGui::BeginDragDropSource())
        {
            SetPayload(script.id, DragDropPayload::Script);
            ImGui::TextUnformatted(script.className.c_str());
            ImGui::EndDragDropSource();
        }
    }

    void BeginFolderDrag(const std::string& folder)
    {
        if (!folder.empty() && ImGui::BeginDragDropSource())
        {
            SetPayload(ScriptLibrary::FolderId(folder), DragDropPayload::ScriptFolder);
            ImGui::TextUnformatted(ScriptFiles::FolderName(folder).c_str());
            ImGui::EndDragDropSource();
        }
    }

    void AcceptMoveDrop(EditorApplication& application, AssetsPanelState& state, const std::string& folder)
    {
        if (!ImGui::BeginDragDropTarget())
        {
            return;
        }
        ScriptLibrary& library = Library(application);
        if (std::optional<Core::Guid> dragged = AcceptPayload(DragDropPayload::Script))
        {
            if (const ScriptEntry* script = library.Find(*dragged))
            {
                if (Core::Result<void, EditorError> moved = ScriptFiles::MoveScript(library.GetRoot(), *script, folder); !moved)
                {
                    Report(moved.GetError());
                }
                library.Refresh();
            }
        }
        if (std::optional<Core::Guid> dragged = AcceptPayload(DragDropPayload::ScriptFolder))
        {
            if (std::optional<std::string> source = library.FindFolder(*dragged))
            {
                Core::Result<std::string, EditorError> moved = ScriptFiles::MoveFolder(library.GetRoot(), *source, folder);
                if (!moved)
                {
                    Report(moved.GetError());
                }
                else if (state.scriptFolder == *source)
                {
                    state.scriptFolder = *moved;
                }
                library.Refresh();
            }
        }
        ImGui::EndDragDropTarget();
    }
}
