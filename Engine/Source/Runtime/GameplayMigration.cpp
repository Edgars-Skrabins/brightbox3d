#include "Runtime/GameplayMigration.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "Framework/LevelGameplaySettings.h"
#include "Runtime/Project.h"

namespace BrightBox3D::Runtime
{
    namespace
    {
        constexpr const char* LevelSettingsKey = "settings";
    }

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
            if (!document.HasMember(LevelSettingsKey) || !document[LevelSettingsKey].IsObject())
            {
                Serialization::SetMember(document, LevelSettingsKey, Serialization::JsonValue(rapidjson::kObjectType), allocator);
            }
            if (Framework::LevelGameplaySettings::MigrateLegacySettings(document[LevelSettingsKey], project.GetLegacyGameMode(), allocator))
            {
                database.SaveJsonAsset(record.guid, document);
                Core::Log::Info("Moved the GameMode setting into level '" + record.name + "'");
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
