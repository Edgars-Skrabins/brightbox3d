#pragma once

#include "Core/Guid.h"
#include "Level/LevelObjectComponent.h"
#include "Level/Transform.h"
#include "Reflection/EngineObject.h"

#include <memory>
#include <string>
#include <type_traits>
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

    enum class ParentTransform
    {
        KeepWorld,
        KeepLocal
    };

    enum class ChildSearch
    {
        Recursive,
        DirectChildren
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
        void SetParent(LevelObject* parent, ParentTransform mode = ParentTransform::KeepWorld, int siblingIndex = -1);
        int GetSiblingIndex() const;
        void SetSiblingIndex(int index);
        bool IsDescendantOf(const LevelObject* ancestor) const;
        LevelObject* GetRoot();
        LevelObject* FindChild(const std::string& name, ChildSearch search = ChildSearch::Recursive) const;

        LevelCollection* GetCollection() const { return m_collection; }

        LevelObjectComponent* AddComponent(const Reflection::TypeInfo& type);
        void RemoveComponent(LevelObjectComponent* component);
        bool MoveComponent(LevelObjectComponent* component, int index);
        const std::vector<std::unique_ptr<LevelObjectComponent>>& GetComponents() const { return m_components; }
        LevelObjectComponent* GetComponent(const Reflection::TypeInfo& type);
        const LevelObjectComponent* GetComponent(const Reflection::TypeInfo& type) const;
        LevelObjectComponent* FindComponentByLocalId(const Core::Guid& localId);
        const LevelObjectComponent* FindComponentByLocalId(const Core::Guid& localId) const;

        template <typename T>
        T* AddComponent()
        {
            return static_cast<T*>(AddComponent(T::StaticType()));
        }

        template <typename T>
        T* GetComponent() { return FindComponent<T>(*this); }

        template <typename T>
        const T* GetComponent() const { return FindComponent<T>(*this); }

        template <typename T>
        std::vector<T*> GetComponents() { return FindComponents<T>(*this); }

        template <typename T>
        std::vector<const T*> GetComponents() const { return FindComponents<T>(*this); }

        template <typename T>
        T* GetComponentInChildren() { return FindComponentInChildren<T>(*this); }

        template <typename T>
        const T* GetComponentInChildren() const { return FindComponentInChildren<T>(*this); }

        template <typename T>
        T* GetComponentInParent() { return FindComponentInParent<T>(*this); }

        template <typename T>
        const T* GetComponentInParent() const { return FindComponentInParent<T>(*this); }

        void Destroy();

        const LevelObjectLink& GetLink() const { return m_state.link; }
        bool IsLinked() const { return m_state.link.asset.IsValid() || m_state.link.sourceId.IsValid(); }
        LevelObject* GetLinkRoot();
        const LevelObject* GetLinkRoot() const;
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

        template <typename T, typename Self>
        using MatchConst = std::conditional_t<std::is_const_v<Self>, const T, T>;

        template <typename T, typename Self>
        static MatchConst<T, Self>* FindComponent(Self& self)
        {
            if constexpr (std::is_same_v<T, Transform>)
            {
                return &self.m_transform;
            }
            else
            {
                return static_cast<MatchConst<T, Self>*>(self.GetComponent(T::StaticType()));
            }
        }

        template <typename T, typename Self>
        static std::vector<MatchConst<T, Self>*> FindComponents(Self& self)
        {
            std::vector<MatchConst<T, Self>*> result;
            for (const auto& component : self.m_components)
            {
                if (component->template IsA<T>() && !component->IsPendingDestroy())
                {
                    result.push_back(static_cast<T*>(component.get()));
                }
            }
            return result;
        }

        template <typename T, typename Self>
        static MatchConst<T, Self>* FindComponentInChildren(Self& self)
        {
            if (MatchConst<T, Self>* component = FindComponent<T>(self))
            {
                return component;
            }
            for (LevelObject* child : self.m_children)
            {
                if (MatchConst<T, Self>* component = FindComponentInChildren<T>(static_cast<MatchConst<LevelObject, Self>&>(*child)))
                {
                    return component;
                }
            }
            return nullptr;
        }

        template <typename T, typename Self>
        static MatchConst<T, Self>* FindComponentInParent(Self& self)
        {
            for (MatchConst<LevelObject, Self>* object = &self; object; object = object->m_parent)
            {
                if (MatchConst<T, Self>* component = FindComponent<T>(*object))
                {
                    return component;
                }
            }
            return nullptr;
        }

        template <typename Self>
        static MatchConst<LevelObjectComponent, Self>* FindComponentOfType(Self& self, const Reflection::TypeInfo& type);
        template <typename Self>
        static MatchConst<LevelObjectComponent, Self>* FindComponentWithLocalId(Self& self, const Core::Guid& localId);
        template <typename Self>
        static MatchConst<LevelObject, Self>* FindLinkRoot(Self& self);

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
