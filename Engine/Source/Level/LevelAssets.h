#pragma once

#include "Assets/JsonAsset.h"

namespace BrightBox3D::Level
{
    BrightClass()
    class LevelAsset : public Assets::JsonAsset
    {
        BrightBody()
    public:
        static constexpr const char* AssetTypeName = "Level";
    };

    BrightClass()
    class LevelCollectionAsset : public Assets::JsonAsset
    {
        BrightBody()
    public:
        static constexpr const char* AssetTypeName = "LevelCollection";
    };

    BrightClass()
    class LevelObjectVariantAsset : public Assets::JsonAsset
    {
        BrightBody()
    public:
        static constexpr const char* AssetTypeName = "LevelObjectVariant";

        const Serialization::JsonValue* GetRoot() const;
        Core::Guid GetParentTemplate() const;
    };

    BrightClass()
    class LevelObjectTemplateAsset : public Assets::JsonAsset
    {
        BrightBody()
    public:
        static constexpr const char* AssetTypeName = "LevelObjectTemplate";

        const Serialization::JsonValue* GetRoot() const;
    };

    void RegisterLevelAssetImporters();
}
