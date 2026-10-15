#include "CreateMenuRegistry.h"

#include "EditorContext.h"
#include "Labels.h"
#include "ObjectIcons.h"
#include "Reflection/TypeRegistry.h"

#include <imgui.h>

#include <algorithm>
#include <set>

namespace BrightBox3D::Editor
{
    CreateMenuRegistry& CreateMenuRegistry::Get()
    {
        static CreateMenuRegistry* registry = new CreateMenuRegistry();
        return *registry;
    }

    void CreateMenuRegistry::Register(CreateMenuEntry entry)
    {
        m_entries.push_back(std::move(entry));
    }

    void CreateMenuRegistry::RegisterPlacement(const Reflection::TypeInfo& type, Placement placement)
    {
        m_placements[&type] = std::move(placement);
    }

    void CreateMenuRegistry::Clear()
    {
        m_entries.clear();
        m_placements.clear();
    }

    CreateMenuRegistry::MenuItem CreateMenuRegistry::ReflectedItem(const Reflection::TypeInfo& type) const
    {
        Icon icon = ObjectIcons::TypeIcon(type);
        auto placement = m_placements.find(&type);
        Placement place = placement == m_placements.end() ? Placement() : placement->second;
        return {
            type.GetCategory(),
            type.GetName(),
            icon == Icon::None ? Icon::FileCode : icon,
            [&type, place](const CreateMenuContext& context)
            {
                Level::LevelObject* object = context.editor.GetObjectEditService().CreateObject(type, type.GetName(), context.parent);
                if (object && place)
                {
                    place(*object, context);
                }
            }
        };
    }

    std::vector<CreateMenuRegistry::MenuItem> CreateMenuRegistry::BuildItems() const
    {
        std::vector<MenuItem> items;
        for (const CreateMenuEntry& entry : m_entries)
        {
            items.push_back({ entry.submenu == EditorText::Count ? std::string() : std::string(Text(entry.submenu)), entry.labelSubject ? Text(entry.label, entry.labelSubject()) : std::string(Text(entry.label)), entry.icon, entry.create });
        }
        std::vector<const Reflection::TypeInfo*> types;
        for (const Reflection::TypeInfo* type : Reflection::TypeRegistry::GetDerived(Level::LevelObject::StaticType()))
        {
            if (type != &Level::LevelObject::StaticType())
            {
                types.push_back(type);
            }
        }
        std::sort(types.begin(), types.end(), [](const Reflection::TypeInfo* a, const Reflection::TypeInfo* b)
        {
            return a->GetCategory() != b->GetCategory() ? a->GetCategory() < b->GetCategory() : a->GetName() < b->GetName();
        });
        for (const Reflection::TypeInfo* type : types)
        {
            items.push_back(ReflectedItem(*type));
        }
        return items;
    }

    void CreateMenuRegistry::Draw(const CreateMenuContext& context) const
    {
        std::vector<MenuItem> items = BuildItems();
        auto drawItem = [&](const MenuItem& item)
        {
            if (ImGui::MenuItem(IconLabel(item.icon, item.label).c_str()))
            {
                item.create(context);
            }
        };
        std::set<std::string> drawnSubmenus;
        for (const MenuItem& item : items)
        {
            if (item.submenu.empty())
            {
                drawItem(item);
                continue;
            }
            if (!drawnSubmenus.insert(item.submenu).second)
            {
                continue;
            }
            if (ImGui::BeginMenu(item.submenu.c_str()))
            {
                for (const MenuItem& member : items)
                {
                    if (member.submenu == item.submenu)
                    {
                        drawItem(member);
                    }
                }
                ImGui::EndMenu();
            }
        }
    }
}
