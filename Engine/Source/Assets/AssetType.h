#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace BrightBox3D::Assets
{
    enum class AssetType : uint8_t
    {
        Unknown,
        Folder,
        Level,
        LevelCollection,
        LevelObjectVariant,
        LevelObjectTemplate,
        Material,
        Model,
        Texture,
        Screen,
        Widget,
        StyleSheet,
        Font,
        Count
    };

    AssetType AssetTypeFromPath(const std::filesystem::path& path);
    const char* AssetTypeName(AssetType type);
    AssetType AssetTypeFromName(const std::string& name);
    const char* AssetTypeExtension(AssetType type);
    std::span<const char* const> AssetTypeExtensions(AssetType type);
}
