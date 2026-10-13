#pragma once

#include "Serialization/Json.h"

#include <string>

namespace BrightBox3D::Reflection
{
    class EngineObject;
    class TypeInfo;
}

namespace BrightBox3D::Framework
{
    enum class GameplaySlot
    {
        GameMode,
        LevelBehavior
    };

    class LevelGameplaySettings
    {
    public:
        static const Reflection::TypeInfo& GetBaseType(GameplaySlot slot);
        static bool IsOptional(GameplaySlot slot);
        static const char* GetKey(GameplaySlot slot);

        static Serialization::JsonValue& GetOrCreateEntry(Serialization::JsonDocument& settings, GameplaySlot slot);
        static const Serialization::JsonValue* FindEntry(const Serialization::JsonValue& settings, GameplaySlot slot);
        static std::string GetClassName(const Serialization::JsonValue& entry);
        static void SetClassName(Serialization::JsonValue& entry, const std::string& className, Serialization::JsonAllocator& allocator);
        static Serialization::JsonValue& GetObjectSettings(Serialization::JsonValue& entry, Serialization::JsonAllocator& allocator);
        static const Serialization::JsonValue* FindObjectSettings(const Serialization::JsonValue& entry);

        static bool MigrateLegacySettings(Serialization::JsonValue& settings, const Serialization::JsonValue* projectGameMode, Serialization::JsonAllocator& allocator);
    };
}
