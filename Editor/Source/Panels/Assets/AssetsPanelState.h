#pragma once

#include "Core/Guid.h"
#include "Widgets/InlineRename.h"

#include <optional>
#include <string>

namespace BrightBox3D::Assets
{
    struct AssetRecord;
}

namespace BrightBox3D::Editor
{
    enum class AssetsRoot
    {
        Assets,
        Scripts,
        EngineClasses
    };

    struct AssetsTarget
    {
        Core::Guid asset;
        std::string folder;
        AssetsRoot root = AssetsRoot::Assets;
        Core::Guid script{};
    };

    struct AssetsPanelState
    {
        InlineRename rename;
        Core::Guid pendingDelete;
        Core::Guid pendingScriptDelete;
        std::optional<AssetsTarget> menuTarget;
        std::string filter;
        AssetsRoot root = AssetsRoot::Assets;
        std::string folder;
        std::string scriptFolder;
        std::string engineModule;
        std::string selectedEngineClass;
        Core::Guid selectedScript;
    };

    AssetsTarget TargetForAsset(const Assets::AssetRecord& record);
}
