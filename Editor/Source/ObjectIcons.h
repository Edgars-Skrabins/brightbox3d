#pragma once

#include "Core/Guid.h"
#include "Core/Icon.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace BrightBox3D::Reflection
{
    class TypeInfo;
}

namespace BrightBox3D::Level
{
    class LevelObject;
}

namespace BrightBox3D::Editor
{
    class ObjectIcons
    {
    public:
        static Core::Icon TypeIcon(const Reflection::TypeInfo& type);
        static Core::Icon ResolveObjectIcon(const Level::LevelObject& object);
        static std::string Label(const Reflection::TypeInfo& type, const std::string& text);

        Core::Icon ObjectIcon(const Level::LevelObject& object);

    private:
        struct Entry
        {
            const Reflection::TypeInfo* type = nullptr;
            size_t componentCount = 0;
            Core::Icon icon = Core::Icon::None;
        };

        std::unordered_map<Core::Guid, Entry> m_cache;
        uint64_t m_registryRevision = 0;
    };
}
