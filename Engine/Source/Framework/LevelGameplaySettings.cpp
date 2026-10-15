#include "Framework/LevelGameplaySettings.h"

#include "Framework/GameModeBase.h"
#include "Level/LevelBehavior.h"
#include "Level/LevelFileSchema.h"
#include "Reflection/TypeRegistry.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Framework
{
    namespace
    {
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
            entry.AddMember(Serialization::JsonValue(Level::LevelFileSchema::GameplayClass, allocator), Serialization::MakeString(className, allocator), allocator);
            entry.AddMember(Serialization::JsonValue(Level::LevelFileSchema::GameplaySettings, allocator), settings, allocator);
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
        return slot == GameplaySlot::GameMode ? Level::LevelFileSchema::GameMode : Level::LevelFileSchema::LevelBehavior;
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
            entry.AddMember(Serialization::JsonValue(Level::LevelFileSchema::GameplayClass, allocator), Serialization::MakeString(IsOptional(slot) ? std::string() : GetBaseType(slot).GetName(), allocator), allocator);
            entry.AddMember(Serialization::JsonValue(Level::LevelFileSchema::GameplaySettings, allocator), Serialization::JsonValue(rapidjson::kObjectType), allocator);
            Serialization::SetMember(settings, GetKey(slot), std::move(entry), allocator);
        }
        return settings[GetKey(slot)];
    }

    const Serialization::JsonValue* LevelGameplaySettings::FindEntry(const Serialization::JsonValue& settings, GameplaySlot slot)
    {
        const Serialization::JsonValue* entry = Serialization::FindMember(settings, GetKey(slot));
        return entry && entry->IsObject() && entry->HasMember(Level::LevelFileSchema::GameplayClass) ? entry : nullptr;
    }

    std::string LevelGameplaySettings::GetClassName(const Serialization::JsonValue& entry)
    {
        return Serialization::GetString(entry, Level::LevelFileSchema::GameplayClass);
    }

    void LevelGameplaySettings::SetClassName(Serialization::JsonValue& entry, const std::string& className, Serialization::JsonAllocator& allocator)
    {
        Serialization::SetMember(entry, Level::LevelFileSchema::GameplayClass, Serialization::MakeString(className, allocator), allocator);
    }

    Serialization::JsonValue& LevelGameplaySettings::GetObjectSettings(Serialization::JsonValue& entry, Serialization::JsonAllocator& allocator)
    {
        if (!FindObjectSettings(entry))
        {
            Serialization::SetMember(entry, Level::LevelFileSchema::GameplaySettings, Serialization::JsonValue(rapidjson::kObjectType), allocator);
        }
        return entry[Level::LevelFileSchema::GameplaySettings];
    }

    const Serialization::JsonValue* LevelGameplaySettings::FindObjectSettings(const Serialization::JsonValue& entry)
    {
        const Serialization::JsonValue* settings = Serialization::FindMember(entry, Level::LevelFileSchema::GameplaySettings);
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
        bool levelOverride = Serialization::GetBool(settings, Level::LevelFileSchema::LegacyOverrideGameMode, false);
        const Serialization::JsonValue* levelGameMode = Serialization::FindMember(settings, Level::LevelFileSchema::GameMode);
        const Serialization::JsonValue* source = levelOverride && levelGameMode ? levelGameMode : projectGameMode;
        bool hadLegacy = settings.HasMember(Level::LevelFileSchema::LegacyOverrideGameMode) || levelGameMode || (projectGameMode && projectGameMode->IsObject());
        if (!hadLegacy)
        {
            return false;
        }
        Serialization::JsonValue entry = MakeEntry(source, allocator);
        settings.RemoveMember(Level::LevelFileSchema::LegacyOverrideGameMode);
        Serialization::SetMember(settings, Level::LevelFileSchema::GameMode, std::move(entry), allocator);
        return true;
    }
}
