#include "Level/LevelObject.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Level/Level.h"
#include "Level/LevelCollection.h"
#include "Reflection/TypeRedirects.h"

#include <algorithm>

namespace BrightBox3D::Level
{
    namespace
    {
        Reflection::TypeRedirectRegistration<LevelObject> s_levelObjectRedirect("LevelObjectInstance");
    }

    LevelObject::LevelObject()
    {
        m_state.id = Core::Guid::Generate();
        m_transform.m_owner = this;
    }

    LevelObject::~LevelObject()
    {
        m_components.clear();
    }

    bool LevelObject::IsActiveInHierarchy() const
    {
        for (const LevelObject* object = this; object; object = object->m_parent)
        {
            if (!object->m_state.isActive)
            {
                return false;
            }
        }
        return true;
    }

    void LevelObject::SetActive(bool active)
    {
        if (m_state.isActive == active)
        {
            return;
        }
        m_state.isActive = active;
        if (GetLevel())
        {
            GetLevel()->RefreshEnabledState(*this);
        }
    }

    void LevelObject::SetParent(LevelObject* parent, bool keepWorldTransform, int siblingIndex)
    {
        if (parent == this || (parent && parent->IsDescendantOf(this)))
        {
            Core::Log::Warning(Core::Text(Core::EngineText::ParentCycle, m_state.name));
            return;
        }
        if (parent && parent->GetLevel() != GetLevel())
        {
            Core::Log::Warning(Core::Text(Core::EngineText::ParentOtherLevel, m_state.name));
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
            if (parent->m_collection != m_collection && GetLevel())
            {
                GetLevel()->MoveToCollection(*this, parent->m_collection);
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

        if (GetLevel())
        {
            GetLevel()->RefreshEnabledState(*this);
            GetLevel()->MarkHierarchyChanged();
        }
    }

    int LevelObject::GetSiblingIndex() const
    {
        const std::vector<LevelObject*>* siblings = nullptr;
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

    void LevelObject::SetSiblingIndex(int index)
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
        if (GetLevel())
        {
            GetLevel()->MarkHierarchyChanged();
        }
    }

    bool LevelObject::IsDescendantOf(const LevelObject* ancestor) const
    {
        for (const LevelObject* object = m_parent; object; object = object->m_parent)
        {
            if (object == ancestor)
            {
                return true;
            }
        }
        return false;
    }

    LevelObject* LevelObject::GetRoot()
    {
        LevelObject* object = this;
        while (object->m_parent)
        {
            object = object->m_parent;
        }
        return object;
    }

    LevelObject* LevelObject::FindChild(const std::string& name, bool recursive) const
    {
        for (LevelObject* child : m_children)
        {
            if (child->m_state.name == name)
            {
                return child;
            }
        }
        if (recursive)
        {
            for (LevelObject* child : m_children)
            {
                if (LevelObject* found = child->FindChild(name, true))
                {
                    return found;
                }
            }
        }
        return nullptr;
    }

    LevelObjectComponent* LevelObject::AddComponent(const Reflection::TypeInfo& type)
    {
        if (!type.IsA(LevelObjectComponent::StaticType()) || type.IsA(Transform::StaticType()))
        {
            Core::Log::Error(Core::Text(Core::EngineText::NotAComponent, type.GetName()));
            return nullptr;
        }
        std::unique_ptr<Reflection::EngineObject> object(type.CreateInstance());
        if (!object)
        {
            Core::Log::Error(Core::Text(Core::EngineText::ComponentAbstract, type.GetName()));
            return nullptr;
        }
        std::unique_ptr<LevelObjectComponent> component(static_cast<LevelObjectComponent*>(object.release()));
        component->m_state.localId = Core::Guid::Generate();
        LevelObjectComponent* result = component.get();
        AttachComponent(std::move(component));
        return result;
    }

    void LevelObject::AttachComponent(std::unique_ptr<LevelObjectComponent> component, int index)
    {
        component->m_owner = this;
        LevelObjectComponent* raw = component.get();
        if (index < 0 || index > static_cast<int>(m_components.size()))
        {
            m_components.push_back(std::move(component));
        }
        else
        {
            m_components.insert(m_components.begin() + index, std::move(component));
        }
        if (GetLevel())
        {
            GetLevel()->OnComponentAttached(*raw);
        }
    }

    std::unique_ptr<LevelObjectComponent> LevelObject::DetachComponent(LevelObjectComponent* component)
    {
        for (auto it = m_components.begin(); it != m_components.end(); ++it)
        {
            if (it->get() == component)
            {
                std::unique_ptr<LevelObjectComponent> detached = std::move(*it);
                m_components.erase(it);
                return detached;
            }
        }
        return nullptr;
    }

    void LevelObject::RemoveComponent(LevelObjectComponent* component)
    {
        if (!component || component == &m_transform || component->m_owner != this)
        {
            return;
        }
        if (GetLevel())
        {
            GetLevel()->RemoveComponent(*component);
            return;
        }
        DetachComponent(component);
    }

    bool LevelObject::MoveComponent(LevelObjectComponent* component, int index)
    {
        std::unique_ptr<LevelObjectComponent> detached = DetachComponent(component);
        if (!detached)
        {
            return false;
        }
        index = std::clamp(index, 0, static_cast<int>(m_components.size()));
        m_components.insert(m_components.begin() + index, std::move(detached));
        return true;
    }

    LevelObjectComponent* LevelObject::GetComponent(const Reflection::TypeInfo& type) const
    {
        if (m_transform.IsA(type))
        {
            return const_cast<Transform*>(&m_transform);
        }
        for (const auto& component : m_components)
        {
            if (component->IsA(type) && !component->IsPendingDestroy())
            {
                return component.get();
            }
        }
        return nullptr;
    }

    LevelObjectComponent* LevelObject::FindComponentByLocalId(const Core::Guid& localId) const
    {
        if (m_transform.m_state.localId == localId)
        {
            return const_cast<Transform*>(&m_transform);
        }
        for (const auto& component : m_components)
        {
            if (component->m_state.localId == localId)
            {
                return component.get();
            }
        }
        return nullptr;
    }

    void LevelObject::Destroy()
    {
        if (GetLevel())
        {
            GetLevel()->DestroyObject(this);
        }
    }

    LevelObject* LevelObject::GetLinkRoot()
    {
        for (LevelObject* object = this; object; object = object->m_parent)
        {
            if (object->m_state.link.isRoot)
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

    void LevelObject::SetMissingType(const std::string& typeName, const std::string& data)
    {
        m_state.missingTypeName = typeName;
        m_state.missingTypeData = data;
    }
}
