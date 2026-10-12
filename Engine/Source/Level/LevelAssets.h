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
    class LevelObjectAsset : public Assets::JsonAsset
    {
        BrightBody()
    public:
        static constexpr const char* AssetTypeName = "LevelObject";

        const Serialization::JsonValue* GetRoot() const;
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
