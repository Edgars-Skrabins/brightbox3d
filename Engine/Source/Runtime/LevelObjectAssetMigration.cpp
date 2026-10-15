#include "Runtime/LevelObjectAssetMigration.h"

#include "Assets/AssetDatabase.h"
#include "Assets/AssetFileSchema.h"
#include "Assets/AssetType.h"
#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Level/LevelFileSchema.h"
#include "Runtime/Project.h"
#include "Serialization/Json.h"

#include <vector>

namespace BrightBox3D::Runtime
{
    namespace
    {
        void SetString(const std::filesystem::path& path, const char* key, const char* value)
        {
            Serialization::JsonDocument document;
            if (Serialization::LoadJsonFile(path, document) && document.IsObject())
            {
                Serialization::SetMember(document, key, Serialization::JsonValue(value, document.GetAllocator()), document.GetAllocator());
                Serialization::SaveJsonFile(path, document);
            }
        }
    }

    int MigrateLegacyLevelObjectAssets(const Project& project)
    {
        std::error_code error;
        std::vector<std::filesystem::path> legacy;
        for (auto it = std::filesystem::recursive_directory_iterator(project.GetAssetsDirectory(), error); !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
        {
            if (it->is_regular_file() && it->path().extension() == Assets::AssetFileSchema::LegacyLevelObjectExtension)
            {
                legacy.push_back(it->path());
            }
        }
        const char* variantName = Assets::AssetTypeName(Assets::AssetType::LevelObjectVariant);
        for (const std::filesystem::path& path : legacy)
        {
            std::filesystem::path renamed = path;
            renamed.replace_extension(Assets::AssetTypeExtension(Assets::AssetType::LevelObjectVariant));
            std::filesystem::rename(path, renamed, error);
            if (error)
            {
                Core::Log::Error(Core::Text(Core::EngineText::VariantMigrationFailed, path.generic_string(), Assets::AssetTypeName(Assets::AssetType::LevelObjectVariant), error.message()));
                continue;
            }
            std::filesystem::path meta = path.string() + Assets::AssetDatabase::MetaExtension;
            std::filesystem::path renamedMeta = renamed.string() + Assets::AssetDatabase::MetaExtension;
            if (std::filesystem::exists(meta))
            {
                std::filesystem::rename(meta, renamedMeta, error);
                SetString(renamedMeta, Assets::AssetFileSchema::Type, variantName);
            }
            SetString(renamed, Level::LevelFileSchema::AssetKind, variantName);
            Core::Log::Info(Core::Text(Core::EngineText::VariantMigrated, path.filename().string(), Assets::AssetTypeName(Assets::AssetType::LevelObjectVariant), renamed.filename().string()));
        }
        return static_cast<int>(legacy.size());
    }
}
