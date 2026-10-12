#pragma once

#include <filesystem>
#include <string>

namespace BrightBox3D::Assets
{
    enum class AssetType
    {
        Unknown,
        Folder,
        Level,
        LevelCollection,
        LevelObject,
        LevelObjectTemplate,
        Material,
        Model,
        Texture,
        Screen,
        Widget,
        StyleSheet,
        Font
    };

    AssetType AssetTypeFromPath(const std::filesystem::path& path);
    const char* AssetTypeName(AssetType type);
    AssetType AssetTypeFromName(const std::string& name);
    const char* AssetTypeExtension(AssetType type);
}
