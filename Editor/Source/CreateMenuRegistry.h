#pragma once

#include "EditorText.h"
#include "IconGlyphs.h"
#include "Math/Vector3.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::Level
{
    class LevelObject;
}

namespace BrightBox3D::Reflection
{
    class TypeInfo;
}

namespace BrightBox3D::Editor
{
    class EditorContext;

    struct CreateMenuContext
    {
        EditorContext& editor;
        Level::LevelObject* parent = nullptr;
        Math::Vector3 spawn;
    };

    struct CreateMenuEntry
    {
        EditorText submenu = EditorText::Count;
        EditorText label = EditorText::Count;
        Icon icon = Icon::None;
        std::function<void(const CreateMenuContext&)> create;
    };

    class CreateMenuRegistry
    {
    public:
        using Placement = std::function<void(Level::LevelObject&, const CreateMenuContext&)>;

        static CreateMenuRegistry& Get();

        void Register(CreateMenuEntry entry);
        void RegisterPlacement(const Reflection::TypeInfo& type, Placement placement);
        void Clear();
        void Draw(const CreateMenuContext& context) const;

    private:
        struct MenuItem
        {
            std::string submenu;
            std::string label;
            Icon icon = Icon::None;
            std::function<void(const CreateMenuContext&)> create;
        };

        std::vector<MenuItem> BuildItems() const;
        MenuItem ReflectedItem(const Reflection::TypeInfo& type) const;

        std::vector<CreateMenuEntry> m_entries;
        std::unordered_map<const Reflection::TypeInfo*, Placement> m_placements;
    };
}
