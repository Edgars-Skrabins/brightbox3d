#pragma once

#include "Core/Guid.h"
#include "Level/LevelObjectComponent.h"
#include "Level/Transform.h"
#include "Reflection/EngineObject.h"

#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class Level;
    class LevelCollection;

    struct LevelObjectLink
    {
        Core::Guid asset;
        Core::Guid sourceId;
        bool isRoot = false;
    };

    struct LevelObjectState
    {
        Core::Guid id;
        std::string name = "LevelObject";
        bool isActive = true;
        LevelObjectLink link;
        Core::Guid ownerId;
        bool hideInLevelContents = false;
        std::string missingTypeName;
        std::string missingTypeData;
    };

    BrightClass(.icon = Icon::CircleDashed)
    class LevelObject : public Reflection::EngineObject
    {
        BrightBody()
    public:
        LevelObject();
        ~LevelObject() override;

        LevelObject(const LevelObject&) = delete;
        LevelObject& operator=(const LevelObject&) = delete;

        const std::string& GetName() const { return m_state.name; }
        void SetName(const std::string& name) { m_state.name = name; }
        const Core::Guid& GetId() const { return m_state.id; }

        bool IsActiveSelf() const { return m_state.isActive; }
        bool IsActiveInHierarchy() const;
        void SetActive(bool active);

        Transform& GetTransform() { return m_transform; }
        const Transform& GetTransform() const { return m_transform; }

        LevelObject* GetParent() const { return m_parent; }
        const std::vector<LevelObject*>& GetChildren() const { return m_children; }
        void SetParent(LevelObject* parent, bool keepWorldTransform = true, int siblingIndex = -1);
        int GetSiblingIndex() const;
        void SetSiblingIndex(int index);
        bool IsDescendantOf(const LevelObject* ancestor) const;
        LevelObject* GetRoot();
        LevelObject* FindChild(const std::string& name, bool recursive = true) const;

        LevelCollection* GetCollection() const { return m_collection; }

        LevelObjectComponent* AddComponent(const Reflection::TypeInfo& type);
        void RemoveComponent(LevelObjectComponent* component);
        bool MoveComponent(LevelObjectComponent* component, int index);
        const std::vector<std::unique_ptr<LevelObjectComponent>>& GetComponents() const { return m_components; }
        LevelObjectComponent* GetComponent(const Reflection::TypeInfo& type) const;
        LevelObjectComponent* FindComponentByLocalId(const Core::Guid& localId) const;

        template <typename T>
        T* AddComponent()
        {
            return static_cast<T*>(AddComponent(T::StaticType()));
        }

        template <typename T>
        T* GetComponent() const
        {
            if constexpr (std::is_same_v<T, Transform>)
            {
                return const_cast<Transform*>(&m_transform);
            }
            else
            {
                return static_cast<T*>(GetComponent(T::StaticType()));
            }
        }

        template <typename T>
        std::vector<T*> GetComponents() const
        {
            std::vector<T*> result;
            for (const auto& component : m_components)
            {
                if (component->IsA<T>() && !component->IsPendingDestroy())
                {
                    result.push_back(static_cast<T*>(component.get()));
                }
            }
            return result;
        }

        template <typename T>
        T* GetComponentInChildren() const
        {
            if (T* component = GetComponent<T>())
            {
                return component;
            }
            for (LevelObject* child : m_children)
            {
                if (T* component = child->GetComponentInChildren<T>())
                {
                    return component;
                }
            }
            return nullptr;
        }

        template <typename T>
        T* GetComponentInParent() const
        {
            for (const LevelObject* object = this; object; object = object->m_parent)
            {
                if (T* component = object->GetComponent<T>())
                {
                    return component;
                }
            }
            return nullptr;
        }

        void Destroy();

        const LevelObjectLink& GetLink() const { return m_state.link; }
        bool IsLinked() const { return m_state.link.asset.IsValid() || m_state.link.sourceId.IsValid(); }
        LevelObject* GetLinkRoot();
        void SetLink(const LevelObjectLink& link) { m_state.link = link; }
        void ClearLink() { m_state.link = {}; }

        const std::string& GetMissingTypeName() const { return m_state.missingTypeName; }
        const std::string& GetMissingTypeData() const { return m_state.missingTypeData; }
        void SetMissingType(const std::string& typeName, const std::string& data);

        bool GetHideInLevelContents() const { return m_state.hideInLevelContents; }
        void SetHideInLevelContents(bool hidden) { m_state.hideInLevelContents = hidden; }
        Core::Guid GetOwnerId() const { return m_state.ownerId; }
        void SetOwnerId(const Core::Guid& ownerId) { m_state.ownerId = ownerId; }

    private:
        friend class Level;

        void AttachComponent(std::unique_ptr<LevelObjectComponent> component, int index = -1);
        std::unique_ptr<LevelObjectComponent> DetachComponent(LevelObjectComponent* component);

        LevelObjectState m_state;
        Transform m_transform;
        std::vector<std::unique_ptr<LevelObjectComponent>> m_components;
        LevelObject* m_parent = nullptr;
        std::vector<LevelObject*> m_children;
        LevelCollection* m_collection = nullptr;
    };
}
