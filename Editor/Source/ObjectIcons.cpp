#include "ObjectIcons.h"

#include "IconGlyphs.h"
#include "Level/LevelObject.h"
#include "Level/Transform.h"
#include "Reflection/TypeRegistry.h"

namespace BrightBox3D::Editor
{
    Core::Icon ObjectIcons::TypeIcon(const Reflection::TypeInfo& type)
    {
        for (const Reflection::TypeInfo* current = &type; current; current = current->GetBase())
        {
            if (current->GetIcon() != Core::Icon::None)
            {
                return current->GetIcon();
            }
        }
        return Core::Icon::None;
    }

    std::string ObjectIcons::Label(const Reflection::TypeInfo& type, const std::string& text)
    {
        Core::Icon icon = TypeIcon(type);
        return icon == Core::Icon::None ? text : std::string(IconGlyph(icon)) + "  " + text;
    }

    Core::Icon ObjectIcons::ResolveObjectIcon(const Level::LevelObject& object)
    {
        const Reflection::TypeInfo& base = Level::LevelObject::StaticType();
        for (const Reflection::TypeInfo* current = &object.GetType(); current && current != &base; current = current->GetBase())
        {
            if (current->GetIcon() != Core::Icon::None)
            {
                return current->GetIcon();
            }
        }
        for (const auto& component : object.GetComponents())
        {
            if (component->IsA<Level::Transform>())
            {
                continue;
            }
            Core::Icon icon = TypeIcon(component->GetType());
            if (icon != Core::Icon::None)
            {
                return icon;
            }
        }
        if (&object.GetType() != &base)
        {
            return Core::Icon::FileCode;
        }
        return TypeIcon(base);
    }

    Core::Icon ObjectIcons::ObjectIcon(const Level::LevelObject& object)
    {
        uint64_t revision = Reflection::TypeRegistry::Revision();
        if (revision != m_registryRevision)
        {
            m_cache.clear();
            m_registryRevision = revision;
        }
        Entry& entry = m_cache[object.GetId()];
        if (entry.type != &object.GetType() || entry.componentCount != object.GetComponents().size() || entry.icon == Core::Icon::None)
        {
            entry.type = &object.GetType();
            entry.componentCount = object.GetComponents().size();
            entry.icon = ResolveObjectIcon(object);
        }
        return entry.icon;
    }
}
