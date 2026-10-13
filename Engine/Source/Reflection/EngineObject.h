#pragma once

#include "Reflection/Macros.h"
#include "Reflection/TypeInfo.h"

#include <unordered_map>
#include <vector>

namespace BrightBox3D::Physics
{
    struct CollisionInfo;
}

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Reflection
{
    struct EngineObjectState
    {
        bool hasLoaded = false;
        bool hasStarted = false;
        bool isEnabledState = false;
        bool isPendingDestroy = false;
    };

    class EngineObject;

    using ReferenceRemap = std::unordered_map<const EngineObject*, EngineObject*>;

    struct ReferenceList
    {
        std::vector<EngineObject*> references;
        std::vector<size_t> counts;
    };

    class ReferenceCarrier
    {
    public:
        enum class Mode
        {
            Save,
            Restore,
            Remap
        };

        ReferenceCarrier(Mode mode, ReferenceList& list, const ReferenceRemap& remap) : m_mode(mode), m_list(list), m_remap(remap) {}

        template <typename T>
        void Carry(T*& reference)
        {
            switch (m_mode)
            {
            case Mode::Save:
                m_list.references.push_back(reference);
                break;
            case Mode::Restore:
                reference = m_cursor < m_list.references.size() ? static_cast<T*>(Resolve(m_list.references[m_cursor++])) : nullptr;
                break;
            case Mode::Remap:
                reference = static_cast<T*>(Resolve(reference));
                break;
            }
        }

        template <typename T>
        void CarryAll(std::vector<T*>& references)
        {
            if (m_mode == Mode::Save)
            {
                m_list.counts.push_back(references.size());
            }
            else if (m_mode == Mode::Restore)
            {
                references.resize(m_countCursor < m_list.counts.size() ? m_list.counts[m_countCursor++] : 0);
            }
            for (T*& reference : references)
            {
                Carry(reference);
            }
        }

    private:
        EngineObject* Resolve(EngineObject* reference) const
        {
            auto it = m_remap.find(reference);
            return it == m_remap.end() ? reference : it->second;
        }

        Mode m_mode;
        ReferenceList& m_list;
        const ReferenceRemap& m_remap;
        size_t m_cursor = 0;
        size_t m_countCursor = 0;
    };

    class EngineObject
    {
    public:
        using CollisionInfo = Physics::CollisionInfo;

        virtual ~EngineObject() = default;

        static const TypeInfo& StaticType();
        virtual const TypeInfo& GetType() const;
        virtual void OnPropertiesChanged() {}
        virtual void CarryReferences(ReferenceCarrier& carrier) {}

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

        bool IsA(const TypeInfo& type) const { return GetType().IsA(type); }

        template <typename T>
        bool IsA() const
        {
            return GetType().IsA(T::StaticType());
        }

        bool HasStarted() const { return m_engineState.hasStarted; }
        bool IsPendingDestroy() const { return m_engineState.isPendingDestroy; }
        virtual Level::Level* GetLevel() const { return m_level; }

    private:
        friend class BrightBox3D::Level::Level;

        EngineObjectState m_engineState;
        Level::Level* m_level = nullptr;
    };

    template <typename T>
    T* Cast(EngineObject* object)
    {
        return object && object->IsA<T>() ? static_cast<T*>(object) : nullptr;
    }

    template <typename T>
    const T* Cast(const EngineObject* object)
    {
        return object && object->IsA<T>() ? static_cast<const T*>(object) : nullptr;
    }
}
