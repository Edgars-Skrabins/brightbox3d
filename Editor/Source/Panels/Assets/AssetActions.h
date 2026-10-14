#pragma once

#include "Assets/AssetType.h"
#include "Core/Guid.h"
#include "Panels/Assets/AssetsPanelState.h"

#include <string>

namespace BrightBox3D::Assets
{
    struct AssetRecord;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;
}

namespace BrightBox3D::Editor::AssetActions
{
    void Open(EditorApplication& application, const Core::Guid& guid);
    bool IsEditable(const AssetsTarget& target);
    bool IsAssetOfType(const AssetsTarget& target, Assets::AssetType type);
    bool CanCreateIn(const std::string& folder);
    void BeginRename(AssetsPanelState& state, const AssetsTarget& target);
    void CommitRename(const Assets::AssetRecord& record, const std::string& name);
    void ShowInFileBrowser(EditorApplication& application, const AssetsTarget& target);
    void Duplicate(EditorApplication& application, const AssetsTarget& target);
    void CopyPath(const AssetsTarget& target);
    std::string DisplayPath(const Assets::AssetRecord& record);
    void AcceptMoveDrop(const std::string& folder);
    void Reimport(const AssetsTarget& target);
    void SetStartupLevel(EditorApplication& application, const AssetsTarget& target);
    void CreateVariant(EditorApplication& application, const AssetsTarget& target);
}
