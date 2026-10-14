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
    struct AssetsTarget
    {
        Core::Guid asset;
        std::string folder;
    };

    struct AssetsPanelState
    {
        InlineRename rename;
        Core::Guid pendingDelete;
        std::optional<AssetsTarget> menuTarget;
    };

    AssetsTarget TargetForAsset(const Assets::AssetRecord& record);
}
