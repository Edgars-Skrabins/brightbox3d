#include "Level/LevelObjectInstance.h"

#include "Core/Log.h"
#include "Level/Level.h"
#include "Level/LevelCollection.h"

#include <algorithm>

namespace BrightBox3D::Level
{
    LevelObjectInstance::LevelObjectInstance()
    {
        m_id = Core::Guid::Generate();
        m_transform.m_owner = this;
    }

    LevelObjectInstance::~LevelObjectInstance()
    {
        m_components.clear();
    }

    bool LevelObjectInstance::IsActiveInHierarchy() const
    {
        for (const LevelObjectInstance* object = this; object; object = object->m_parent)
        {
            if (!object->m_isActive)
            {
                return false;
            }
        }
        return true;
    }

    void LevelObjectInstance::SetActive(bool active)
    {
        if (m_isActive == active)
        {
            return;
        }
        m_isActive = active;
        if (m_level)
        {
            m_level->RefreshEnabledState(*this);
        }
    }

    void LevelObjectInstance::SetParent(LevelObjectInstance* parent, bool keepWorldTransform, int siblingIndex)
    {
        if (parent == this || (parent && parent->IsDescendantOf(this)))
        {
            Core::Log::Warning("Cannot parent '" + m_name + "' to itself or one of its descendants");
            return;
        }
        if (parent && parent->m_level != m_level)
        {
            Core::Log::Warning("Cannot parent '" + m_name + "' to an object in another level");
            return;
        }

        Math::Matrix4 world = m_transform.GetWorldMatrix();

        if (m_parent)
        {
            auto& siblings = m_parent->m_children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        }
        else if (m_collection)
        {
            m_collection->RemoveRoot(this);
        }

        m_parent = parent;
        if (parent)
        {
            auto& siblings = parent->m_children;
            if (siblingIndex < 0 || siblingIndex > static_cast<int>(siblings.size()))
            {
                siblings.push_back(this);
            }
            else
            {
                siblings.insert(siblings.begin() + siblingIndex, this);
            }
            if (parent->m_collection != m_collection && m_level)
            {
                m_level->MoveToCollection(*this, parent->m_collection);
            }
        }
        else if (m_collection)
        {
            m_collection->AddRoot(this, siblingIndex);
        }

        if (keepWorldTransform)
        {
            m_transform.SetWorldMatrix(world);
        }
        else
        {
            m_transform.MarkDirty();
        }

        if (m_level)
        {
            m_level->RefreshEnabledState(*this);
            m_level->MarkHierarchyChanged();
        }
    }

    int LevelObjectInstance::GetSiblingIndex() const
    {
        const std::vector<LevelObjectInstance*>* siblings = nullptr;
        if (m_parent)
        {
            siblings = &m_parent->m_children;
        }
        else if (m_collection)
        {
            siblings = &m_collection->GetRoots();
        }
        if (!siblings)
        {
            return 0;
        }
        auto it = std::find(siblings->begin(), siblings->end(), this);
        return it == siblings->end() ? 0 : static_cast<int>(it - siblings->begin());
    }

    void LevelObjectInstance::SetSiblingIndex(int index)
    {
        if (m_parent)
        {
            auto& siblings = m_parent->m_children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
            index = std::clamp(index, 0, static_cast<int>(siblings.size()));
            siblings.insert(siblings.begin() + index, this);
        }
        else if (m_collection)
        {
            m_collection->RemoveRoot(this);
            m_collection->AddRoot(this, index);
        }
        if (m_level)
        {
            m_level->MarkHierarchyChanged();
        }
    }

    bool LevelObjectInstance::IsDescendantOf(const LevelObjectInstance* ancestor) const
    {
        for (const LevelObjectInstance* object = m_parent; object; object = object->m_parent)
        {
            if (object == ancestor)
            {
                return true;
            }
        }
        return false;
    }

    LevelObjectInstance* LevelObjectInstance::GetRoot()
    {
        LevelObjectInstance* object = this;
        while (object->m_parent)
        {
            object = object->m_parent;
        }
        return object;
    }

    LevelObjectInstance* LevelObjectInstance::FindChild(const std::string& name, bool recursive) const
    {
        for (LevelObjectInstance* child : m_children)
        {
            if (child->m_name == name)
            {
                return child;
            }
        }
        if (recursive)
        {
            for (LevelObjectInstance* child : m_children)
            {
                if (LevelObjectInstance* found = child->FindChild(name, true))
                {
                    return found;
                }
            }
        }
        return nullptr;
    }

    Component* LevelObjectInstance::AddComponent(const Reflection::TypeInfo& type)
    {
        if (!type.IsA(Component::StaticType()) || type.IsA(Transform::StaticType()))
        {
            Core::Log::Error("'" + type.GetName() + "' cannot be added as a component");
            return nullptr;
        }
        std::unique_ptr<Reflection::Object> object(type.CreateInstance());
        if (!object)
        {
            Core::Log::Error("Component type '" + type.GetName() + "' is abstract");
            return nullptr;
        }
        std::unique_ptr<Component> component(static_cast<Component*>(object.release()));
        component->m_localId = Core::Guid::Generate();
        Component* result = component.get();
        AttachComponent(std::move(component));
        return result;
    }

    void LevelObjectInstance::AttachComponent(std::unique_ptr<Component> component, int index)
    {
        component->m_owner = this;
        Component* raw = component.get();
        if (index < 0 || index > static_cast<int>(m_components.size()))
        {
            m_components.push_back(std::move(component));
        }
        else
        {
            m_components.insert(m_components.begin() + index, std::move(component));
        }
        if (m_level)
        {
            m_level->OnComponentAttached(*raw);
        }
    }

    std::unique_ptr<Component> LevelObjectInstance::DetachComponent(Component* component)
    {
        for (auto it = m_components.begin(); it != m_components.end(); ++it)
        {
            if (it->get() == component)
            {
                std::unique_ptr<Component> detached = std::move(*it);
                m_components.erase(it);
                return detached;
            }
        }
        return nullptr;
    }

    void LevelObjectInstance::RemoveComponent(Component* component)
    {
        if (!component || component == &m_transform || component->m_owner != this)
        {
            return;
        }
        if (m_level)
        {
            m_level->RemoveComponent(*component);
            return;
        }
        DetachComponent(component);
    }

    bool LevelObjectInstance::MoveComponent(Component* component, int index)
    {
        std::unique_ptr<Component> detached = DetachComponent(component);
        if (!detached)
        {
            return false;
        }
        index = std::clamp(index, 0, static_cast<int>(m_components.size()));
        m_components.insert(m_components.begin() + index, std::move(detached));
        return true;
    }

    Component* LevelObjectInstance::GetComponent(const Reflection::TypeInfo& type) const
    {
        if (m_transform.IsA(type))
        {
            return const_cast<Transform*>(&m_transform);
        }
        for (const auto& component : m_components)
        {
            if (component->IsA(type) && !component->m_isPendingDestroy)
            {
                return component.get();
            }
        }
        return nullptr;
    }

    Component* LevelObjectInstance::FindComponentByLocalId(const Core::Guid& localId) const
    {
        if (m_transform.m_localId == localId)
        {
            return const_cast<Transform*>(&m_transform);
        }
        for (const auto& component : m_components)
        {
            if (component->m_localId == localId)
            {
                return component.get();
            }
        }
        return nullptr;
    }

    void LevelObjectInstance::Destroy()
    {
        if (m_level)
        {
            m_level->DestroyObject(this);
        }
    }

    LevelObjectInstance* LevelObjectInstance::GetLinkRoot()
    {
        for (LevelObjectInstance* object = this; object; object = object->m_parent)
        {
            if (object->m_link.isRoot)
            {
                return object;
            }
            if (!object->IsLinked())
            {
                return nullptr;
            }
        }
        return nullptr;
    }

    void LevelObjectInstance::SetMissingType(const std::string& typeName, const std::string& data)
    {
        m_missingTypeName = typeName;
        m_missingTypeData = data;
    }
}
