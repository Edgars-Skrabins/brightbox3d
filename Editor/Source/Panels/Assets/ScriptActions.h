#pragma once

#include "IconGlyphs.h"
#include "Panels/Assets/AssetsPanelState.h"

#include <string>

namespace BrightBox3D::Editor
{
    class EditorApplication;
    struct ScriptEntry;
}

namespace BrightBox3D::Editor::ScriptActions
{
    AssetsTarget TargetForScript(const ScriptEntry& script);
    AssetsTarget TargetForFolder(const std::string& folder);
    Icon ScriptIcon(const ScriptEntry& script);
    std::string ScriptKind(const ScriptEntry& script);
    std::string DisplayPath(const AssetsTarget& target);

    void Open(EditorApplication& application, AssetsPanelState& state, const AssetsTarget& target);
    bool HasHeader(EditorApplication& application, const AssetsTarget& target);
    void OpenHeader(EditorApplication& application, const AssetsTarget& target);
    void ShowInFileBrowser(EditorApplication& application, const AssetsTarget& target);
    void CopyPath(const AssetsTarget& target);
    void CreateFolder(EditorApplication& application, AssetsPanelState& state, const std::string& parent);
    void BeginRename(EditorApplication& application, AssetsPanelState& state, const AssetsTarget& target);
    void CommitScriptRename(EditorApplication& application, AssetsPanelState& state, const ScriptEntry& script, const std::string& name);
    void DeleteScript(EditorApplication& application, AssetsPanelState& state, const Core::Guid& script);
    void CommitFolderRename(EditorApplication& application, AssetsPanelState& state, const std::string& folder, const std::string& name);
    void BeginDrag(const ScriptEntry& script);
    void BeginFolderDrag(const std::string& folder);
    void AcceptMoveDrop(EditorApplication& application, AssetsPanelState& state, const std::string& folder);
}
