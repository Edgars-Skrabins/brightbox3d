#include "Assets/AssetType.h"

#include "Core/StringUtils.h"

namespace BrightBox3D::Assets
{
    namespace
    {
        struct TypeEntry
        {
            AssetType type;
            const char* name;
            const char* extension;
        };

        const TypeEntry s_entries[] = {
            { AssetType::Unknown, "Unknown", "" },
            { AssetType::Folder, "Folder", "" },
            { AssetType::Level, "Level", ".level" },
            { AssetType::LevelCollection, "LevelCollection", ".collection" },
            { AssetType::LevelObject, "LevelObject", ".levelobject" },
            { AssetType::LevelObjectTemplate, "LevelObjectTemplate", ".template" },
            { AssetType::Material, "Material", ".material" },
            { AssetType::Model, "Model", ".fbx" },
            { AssetType::Texture, "Texture", ".png" },
            { AssetType::Screen, "Screen", ".screen.xml" },
            { AssetType::Widget, "Widget", ".widget.xml" },
            { AssetType::StyleSheet, "StyleSheet", ".css" },
            { AssetType::Font, "Font", ".ttf" }
        };
    }

    AssetType AssetTypeFromPath(const std::filesystem::path& path)
    {
        std::string name = Core::ToLower(path.filename().string());
        if (Core::EndsWith(name, ".screen.xml"))
        {
            return AssetType::Screen;
        }
        if (Core::EndsWith(name, ".widget.xml"))
        {
            return AssetType::Widget;
        }
        std::string extension = Core::ToLower(path.extension().string());
        if (extension == ".level")
        {
            return AssetType::Level;
        }
        if (extension == ".collection")
        {
            return AssetType::LevelCollection;
        }
        if (extension == ".levelobject")
        {
            return AssetType::LevelObject;
        }
        if (extension == ".template")
        {
            return AssetType::LevelObjectTemplate;
        }
        if (extension == ".material")
        {
            return AssetType::Material;
        }
        if (extension == ".fbx")
        {
            return AssetType::Model;
        }
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" || extension == ".bmp" || extension == ".hdr")
        {
            return AssetType::Texture;
        }
        if (extension == ".css")
        {
            return AssetType::StyleSheet;
        }
        if (extension == ".ttf" || extension == ".otf")
        {
            return AssetType::Font;
        }
        return AssetType::Unknown;
    }

    const char* AssetTypeName(AssetType type)
    {
        for (const TypeEntry& entry : s_entries)
        {
            if (entry.type == type)
            {
                return entry.name;
            }
        }
        return "Unknown";
    }

    AssetType AssetTypeFromName(const std::string& name)
    {
        for (const TypeEntry& entry : s_entries)
        {
            if (name == entry.name)
            {
                return entry.type;
            }
        }
        return AssetType::Unknown;
    }

    const char* AssetTypeExtension(AssetType type)
    {
        for (const TypeEntry& entry : s_entries)
        {
            if (entry.type == type)
            {
                return entry.extension;
            }
        }
        return "";
    }
}
