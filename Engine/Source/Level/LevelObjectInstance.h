#pragma once

#include "Core/Guid.h"
#include "Level/Component.h"
#include "Level/Transform.h"
#include "Reflection/Object.h"

#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class Level;
    class LevelCollection;
    class LevelSerializerAccess;
    class ObjectReplacer;

    struct LevelObjectLink
    {
        Core::Guid asset;
        Core::Guid sourceId;
        bool isRoot = false;
    };

    BrightClass()
    class LevelObjectInstance : public Reflection::Object
    {
        BrightBody()
    public:
        LevelObjectInstance();
        ~LevelObjectInstance() override;

        LevelObjectInstance(const LevelObjectInstance&) = delete;
        LevelObjectInstance& operator=(const LevelObjectInstance&) = delete;

        virtual void OnInitialize() {}
        virtual void OnGameLoad() {}
        virtual void OnGameStart() {}
        virtual void OnGameUpdate(float deltaTime) {}
        virtual void OnGameUpdateFixed(float fixedDeltaTime) {}
        virtual void OnGameUpdateLate(float deltaTime) {}
        virtual void OnEnable() {}
        virtual void OnDisable() {}
        virtual void OnGameEnd() {}
        virtual void OnDestroy() {}
        virtual void OnCollisionEnter(const CollisionInfo& collision) {}
        virtual void OnCollisionExit(const CollisionInfo& collision) {}
        virtual void OnTriggerEnter(const CollisionInfo& collision) {}
        virtual void OnTriggerExit(const CollisionInfo& collision) {}

        const std::string& GetName() const { return m_name; }
        void SetName(const std::string& name) { m_name = name; }
        const Core::Guid& GetId() const { return m_id; }

        bool IsActiveSelf() const { return m_isActive; }
        bool IsActiveInHierarchy() const;
        void SetActive(bool active);

        Transform& GetTransform() { return m_transform; }
        const Transform& GetTransform() const { return m_transform; }

        LevelObjectInstance* GetParent() const { return m_parent; }
        const std::vector<LevelObjectInstance*>& GetChildren() const { return m_children; }
        void SetParent(LevelObjectInstance* parent, bool keepWorldTransform = true, int siblingIndex = -1);
        int GetSiblingIndex() const;
        void SetSiblingIndex(int index);
        bool IsDescendantOf(const LevelObjectInstance* ancestor) const;
        LevelObjectInstance* GetRoot();
        LevelObjectInstance* FindChild(const std::string& name, bool recursive = true) const;

        Level* GetLevel() const { return m_level; }
        LevelCollection* GetCollection() const { return m_collection; }

        Component* AddComponent(const Reflection::TypeInfo& type);
        void RemoveComponent(Component* component);
        bool MoveComponent(Component* component, int index);
        const std::vector<std::unique_ptr<Component>>& GetComponents() const { return m_components; }
        Component* GetComponent(const Reflection::TypeInfo& type) const;
        Component* FindComponentByLocalId(const Core::Guid& localId) const;

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
            for (LevelObjectInstance* child : m_children)
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
            for (const LevelObjectInstance* object = this; object; object = object->m_parent)
            {
                if (T* component = object->GetComponent<T>())
                {
                    return component;
                }
            }
            return nullptr;
        }

        void Destroy();
        bool IsPendingDestroy() const { return m_isPendingDestroy; }
        bool HasStarted() const { return m_hasStarted; }

        const LevelObjectLink& GetLink() const { return m_link; }
        bool IsLinked() const { return m_link.asset.IsValid() || m_link.sourceId.IsValid(); }
        LevelObjectInstance* GetLinkRoot();
        void SetLink(const LevelObjectLink& link) { m_link = link; }
        void ClearLink() { m_link = {}; }

        const std::string& GetMissingTypeName() const { return m_missingTypeName; }
        const std::string& GetMissingTypeData() const { return m_missingTypeData; }
        void SetMissingType(const std::string& typeName, const std::string& data);

        bool GetHideInLevelContents() const { return m_hideInLevelContents; }
        void SetHideInLevelContents(bool hidden) { m_hideInLevelContents = hidden; }
        Core::Guid GetOwnerId() const { return m_ownerId; }
        void SetOwnerId(const Core::Guid& ownerId) { m_ownerId = ownerId; }

    private:
        friend class Level;
        friend class LevelSerializer;
        friend class LevelSerializerAccess;
        friend class ObjectReplacer;

        void SetId(const Core::Guid& id) { m_id = id; }
        void AttachComponent(std::unique_ptr<Component> component, int index = -1);
        std::unique_ptr<Component> DetachComponent(Component* component);

        std::string m_name = "LevelObject";
        Core::Guid m_id;
        bool m_isActive = true;
        Transform m_transform;
        std::vector<std::unique_ptr<Component>> m_components;
        LevelObjectInstance* m_parent = nullptr;
        std::vector<LevelObjectInstance*> m_children;
        Level* m_level = nullptr;
        LevelCollection* m_collection = nullptr;
        LevelObjectLink m_link;
        std::string m_missingTypeName;
        std::string m_missingTypeData;
        Core::Guid m_ownerId;
        bool m_hideInLevelContents = false;
        bool m_hasLoaded = false;
        bool m_hasStarted = false;
        bool m_isEnabledState = false;
        bool m_isPendingDestroy = false;
    };
}
