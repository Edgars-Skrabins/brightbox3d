#include "Level/LevelAssets.h"

#include "Assets/AssetDatabase.h"
#include "Level/LevelFileSchema.h"

namespace BrightBox3D::Level
{
    namespace
    {
        template <typename T>
        class TypedJsonImporter : public Assets::JsonAssetImporter
        {
        protected:
            std::shared_ptr<Assets::Asset> CreateAsset(Serialization::JsonDocument&& document, std::string& error) override
            {
                auto asset = std::make_shared<T>();
                asset->GetDocument().Swap(document);
                return asset;
            }
        };

        struct ImporterRegistration
        {
            ImporterRegistration()
            {
                RegisterLevelAssetImporters();
            }
        };

        ImporterRegistration s_importerRegistration;
    }

    const Serialization::JsonValue* LevelObjectVariantAsset::GetRoot() const
    {
        return Serialization::FindMember(GetDocument(), LevelFileSchema::Root);
    }

    Core::Guid LevelObjectVariantAsset::GetParentTemplate() const
    {
        const Serialization::JsonValue* root = GetRoot();
        return root ? Core::Guid::Parse(Serialization::GetString(*root, LevelFileSchema::LevelObjectAsset)) : Core::Guid{};
    }

    const Serialization::JsonValue* LevelObjectTemplateAsset::GetRoot() const
    {
        return Serialization::FindMember(GetDocument(), LevelFileSchema::Root);
    }

    void RegisterLevelAssetImporters()
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        database.RegisterImporter(Assets::AssetType::Level, std::make_shared<TypedJsonImporter<LevelAsset>>());
        database.RegisterImporter(Assets::AssetType::LevelCollection, std::make_shared<TypedJsonImporter<LevelCollectionAsset>>());
        database.RegisterImporter(Assets::AssetType::LevelObjectVariant, std::make_shared<TypedJsonImporter<LevelObjectVariantAsset>>());
        database.RegisterImporter(Assets::AssetType::LevelObjectTemplate, std::make_shared<TypedJsonImporter<LevelObjectTemplateAsset>>());
    }
}
