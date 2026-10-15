#include "Runtime/GameplayMigration.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Framework/LevelGameplaySettings.h"
#include "Level/LevelFileSchema.h"
#include "Runtime/Project.h"

namespace BrightBox3D::Runtime
{
    bool MigrateLegacyGameplaySettings(Project& project)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        bool migrated = false;
        for (const Assets::AssetRecord& record : database.GetRecords(Assets::AssetType::Level))
        {
            Serialization::JsonDocument document;
            if (!Serialization::LoadJsonFile(database.GetAbsolutePath(record), document) || !document.IsObject())
            {
                continue;
            }
            auto& allocator = document.GetAllocator();
            if (!document.HasMember(Level::LevelFileSchema::Settings) || !document[Level::LevelFileSchema::Settings].IsObject())
            {
                Serialization::SetMember(document, Level::LevelFileSchema::Settings, Serialization::JsonValue(rapidjson::kObjectType), allocator);
            }
            if (Framework::LevelGameplaySettings::MigrateLegacySettings(document[Level::LevelFileSchema::Settings], project.GetLegacyGameMode(), allocator))
            {
                database.SaveJsonAsset(record.guid, document);
                Core::Log::Info(Core::Text(Core::EngineText::GameModeMigrated, record.name));
                migrated = true;
            }
        }
        if (project.GetLegacyGameMode())
        {
            project.ClearLegacyGameMode();
            project.Save();
            migrated = true;
        }
        return migrated;
    }
}
