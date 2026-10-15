#include "Assets/AssetType.h"

#include "Core/EngineText.h"
#include "Core/StringUtils.h"

#include <iterator>
#include <span>
#include <string_view>

namespace BrightBox3D::Assets
{
    namespace
    {
        constexpr const char* NoExtensions[] = { "" };
        constexpr const char* LevelExtensions[] = { ".level" };
        constexpr const char* CollectionExtensions[] = { ".collection" };
        constexpr const char* VariantExtensions[] = { ".variant" };
        constexpr const char* TemplateExtensions[] = { ".template" };
        constexpr const char* MaterialExtensions[] = { ".material" };
        constexpr const char* ModelExtensions[] = { ".fbx" };
        constexpr const char* TextureExtensions[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr" };
        constexpr const char* ScreenExtensions[] = { ".screen.xml" };
        constexpr const char* WidgetExtensions[] = { ".widget.xml" };
        constexpr const char* StyleSheetExtensions[] = { ".css" };
        constexpr const char* FontExtensions[] = { ".ttf", ".otf" };

        struct AssetTypeInfo
        {
            const char* name;
            std::span<const char* const> extensions;
            Core::EngineText newName = Core::EngineText::Count;
        };

        constexpr AssetTypeInfo TypeInfos[] = {
            { "Unknown", NoExtensions },
            { "Folder", NoExtensions, Core::EngineText::NewFolderName },
            { "Level", LevelExtensions, Core::EngineText::NewLevelName },
            { "LevelCollection", CollectionExtensions, Core::EngineText::NewCollectionName },
            { "LevelObjectVariant", VariantExtensions },
            { "LevelObjectTemplate", TemplateExtensions, Core::EngineText::NewTemplateName },
            { "Material", MaterialExtensions, Core::EngineText::NewMaterialName },
            { "Model", ModelExtensions },
            { "Texture", TextureExtensions },
            { "Screen", ScreenExtensions, Core::EngineText::NewScreenName },
            { "Widget", WidgetExtensions, Core::EngineText::NewWidgetName },
            { "StyleSheet", StyleSheetExtensions, Core::EngineText::NewStyleSheetName },
            { "Font", FontExtensions },
        };

        static_assert(std::size(TypeInfos) == static_cast<size_t>(AssetType::Count));

        const AssetTypeInfo& Info(AssetType type)
        {
            return TypeInfos[static_cast<size_t>(type)];
        }
    }

    AssetType AssetTypeFromPath(const std::filesystem::path& path)
    {
        std::string name = Core::ToLower(path.filename().string());
        AssetType match = AssetType::Unknown;
        size_t matchLength = 0;
        for (size_t index = 0; index < std::size(TypeInfos); ++index)
        {
            for (std::string_view extension : TypeInfos[index].extensions)
            {
                if (extension.size() > matchLength && Core::EndsWith(name, extension))
                {
                    match = static_cast<AssetType>(index);
                    matchLength = extension.size();
                }
            }
        }
        return match;
    }

    const char* AssetTypeName(AssetType type)
    {
        return Info(type).name;
    }

    AssetType AssetTypeFromName(const std::string& name)
    {
        for (size_t index = 0; index < std::size(TypeInfos); ++index)
        {
            if (name == TypeInfos[index].name)
            {
                return static_cast<AssetType>(index);
            }
        }
        return AssetType::Unknown;
    }

    const char* AssetTypeExtension(AssetType type)
    {
        return Info(type).extensions.front();
    }

    std::span<const char* const> AssetTypeExtensions(AssetType type)
    {
        return Info(type).extensions;
    }

    bool IsCreatableAssetType(AssetType type)
    {
        return Info(type).newName != Core::EngineText::Count;
    }

    std::string NewAssetName(AssetType type)
    {
        return IsCreatableAssetType(type) ? std::string(Core::Text(Info(type).newName)) : std::string(AssetTypeName(type));
    }

    std::string_view MatchedExtension(AssetType type, std::string_view fileName)
    {
        std::string lower = Core::ToLower(fileName);
        std::string_view match;
        for (std::string_view extension : Info(type).extensions)
        {
            if (extension.size() > match.size() && Core::EndsWith(lower, extension))
            {
                match = fileName.substr(fileName.size() - extension.size());
            }
        }
        return match;
    }
}
