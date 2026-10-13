#include "Framework/LevelGameplaySettings.h"

#include "Framework/GameModeBase.h"
#include "Level/LevelBehavior.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Framework
{
    namespace
    {
        constexpr const char* GameModeKey = "gameMode";
        constexpr const char* LevelBehaviorKey = "levelBehavior";
        constexpr const char* ClassKey = "class";
        constexpr const char* SettingsKey = "settings";
        constexpr const char* LegacyOverrideKey = "overrideGameMode";

        std::string CanonicalClassName(const std::string& name)
        {
            const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(name);
            return type ? type->GetName() : name;
        }

        Serialization::JsonValue MakeEntry(const Serialization::JsonValue* legacy, Serialization::JsonAllocator& allocator)
        {
            Serialization::JsonValue entry(rapidjson::kObjectType);
            Serialization::JsonValue settings(rapidjson::kObjectType);
            std::string className = GameModeBase::StaticType().GetName();
            if (legacy && legacy->IsObject())
            {
                className = CanonicalClassName(Serialization::GetString(*legacy, Serialization::TypeKey, className));
                for (auto member = legacy->MemberBegin(); member != legacy->MemberEnd(); ++member)
                {
                    if (std::string(member->name.GetString()) != Serialization::TypeKey)
                    {
                        settings.AddMember(Serialization::JsonValue(member->name, allocator), Serialization::JsonValue(member->value, allocator), allocator);
                    }
                }
            }
            entry.AddMember(Serialization::JsonValue(ClassKey, allocator), Serialization::MakeString(className, allocator), allocator);
            entry.AddMember(Serialization::JsonValue(SettingsKey, allocator), settings, allocator);
            return entry;
        }
    }

    const Reflection::TypeInfo& LevelGameplaySettings::GetBaseType(GameplaySlot slot)
    {
        return slot == GameplaySlot::GameMode ? GameModeBase::StaticType() : Level::LevelBehavior::StaticType();
    }

    bool LevelGameplaySettings::IsOptional(GameplaySlot slot)
    {
        return slot == GameplaySlot::LevelBehavior;
    }

    const char* LevelGameplaySettings::GetKey(GameplaySlot slot)
    {
        return slot == GameplaySlot::GameMode ? GameModeKey : LevelBehaviorKey;
    }

    Serialization::JsonValue& LevelGameplaySettings::GetOrCreateEntry(Serialization::JsonDocument& settings, GameplaySlot slot)
    {
        if (!settings.IsObject())
        {
            settings.SetObject();
        }
        auto& allocator = settings.GetAllocator();
        if (!FindEntry(settings, slot))
        {
            Serialization::JsonValue entry(rapidjson::kObjectType);
            entry.AddMember(Serialization::JsonValue(ClassKey, allocator), Serialization::MakeString(IsOptional(slot) ? std::string() : GetBaseType(slot).GetName(), allocator), allocator);
            entry.AddMember(Serialization::JsonValue(SettingsKey, allocator), Serialization::JsonValue(rapidjson::kObjectType), allocator);
            Serialization::SetMember(settings, GetKey(slot), std::move(entry), allocator);
        }
        return settings[GetKey(slot)];
    }

    const Serialization::JsonValue* LevelGameplaySettings::FindEntry(const Serialization::JsonValue& settings, GameplaySlot slot)
    {
        const Serialization::JsonValue* entry = Serialization::FindMember(settings, GetKey(slot));
        return entry && entry->IsObject() && entry->HasMember(ClassKey) ? entry : nullptr;
    }

    std::string LevelGameplaySettings::GetClassName(const Serialization::JsonValue& entry)
    {
        return Serialization::GetString(entry, ClassKey);
    }

    void LevelGameplaySettings::SetClassName(Serialization::JsonValue& entry, const std::string& className, Serialization::JsonAllocator& allocator)
    {
        Serialization::SetMember(entry, ClassKey, Serialization::MakeString(className, allocator), allocator);
    }

    Serialization::JsonValue& LevelGameplaySettings::GetObjectSettings(Serialization::JsonValue& entry, Serialization::JsonAllocator& allocator)
    {
        if (!FindObjectSettings(entry))
        {
            Serialization::SetMember(entry, SettingsKey, Serialization::JsonValue(rapidjson::kObjectType), allocator);
        }
        return entry[SettingsKey];
    }

    const Serialization::JsonValue* LevelGameplaySettings::FindObjectSettings(const Serialization::JsonValue& entry)
    {
        const Serialization::JsonValue* settings = Serialization::FindMember(entry, SettingsKey);
        return settings && settings->IsObject() ? settings : nullptr;
    }

    bool LevelGameplaySettings::MigrateLegacySettings(Serialization::JsonValue& settings, const Serialization::JsonValue* projectGameMode, Serialization::JsonAllocator& allocator)
    {
        if (!settings.IsObject())
        {
            settings.SetObject();
        }
        if (FindEntry(settings, GameplaySlot::GameMode))
        {
            return false;
        }
        bool levelOverride = Serialization::GetBool(settings, LegacyOverrideKey, false);
        const Serialization::JsonValue* levelGameMode = Serialization::FindMember(settings, GameModeKey);
        const Serialization::JsonValue* source = levelOverride && levelGameMode ? levelGameMode : projectGameMode;
        bool hadLegacy = settings.HasMember(LegacyOverrideKey) || levelGameMode || (projectGameMode && projectGameMode->IsObject());
        if (!hadLegacy)
        {
            return false;
        }
        Serialization::JsonValue entry = MakeEntry(source, allocator);
        settings.RemoveMember(LegacyOverrideKey);
        Serialization::SetMember(settings, GameModeKey, std::move(entry), allocator);
        return true;
    }
}
