#pragma once

#include "Level/LevelObjectLinking.h"
#include "Serialization/Json.h"

#include <functional>
#include <string>

namespace BrightBox3D::Level
{
    class LevelObject;
    class LevelObjectComponent;
}

namespace BrightBox3D::Level::LevelOverrides
{
    using OverrideVisitor = std::function<void(const LevelObjectComponent* live, const LevelObjectComponent* prototype, const std::string& property, bool equal)>;

    bool IsPseudoProperty(const std::string& property);
    bool PseudoPropertiesEqual(const LevelObject& a, const LevelObjectComponent* componentA, const LevelObject& b, const LevelObjectComponent* componentB, const std::string& property);
    void WritePseudoProperty(const LevelObject& object, const LevelObjectComponent* component, const std::string& property, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
    void ReadPseudoProperty(LevelObject& object, LevelObjectComponent* component, const std::string& property, const Serialization::JsonValue& value);

    std::string ComponentKey(const LevelObject& object, const LevelObjectComponent* component);
    LevelObjectComponent* ResolveComponent(LevelObject& object, const std::string& key);

    void ForEachOverrideCandidate(const LevelObject& live, const LevelObject& prototype, bool isRoot, const OverrideVisitor& visitor);
    void WriteOverrides(const LevelObject& linkRoot, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
    void ApplyOverrides(const Serialization::JsonValue& overrides, const LevelObjectLinking::SourceIndex& bySource);
}
