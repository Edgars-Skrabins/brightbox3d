#pragma once

#include "Assets/AssetDatabase.h"
#include "Core/Event.h"
#include "Core/Guid.h"
#include "Level/LevelAssets.h"
#include "Level/LevelCollection.h"
#include "Level/LevelObject.h"
#include "Level/LevelSubsystem.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "Serialization/Json.h"

#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace BrightBox3D::Level
{
    struct LevelObjectRuntimeState
    {
        Reflection::EngineObjectState engine;
        LevelObjectState object;
    };

    struct ComponentRuntimeState
    {
        Reflection::EngineObjectState engine;
        ComponentState component;
    };

    class Level
    {
    public:
        static constexpr const char* RuntimeCollectionName = "Runtime";

        explicit Level(std::string name = "Level");
        ~Level();

        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;

        static std::vector<Level*> GetAllLevels();

        LevelCollection* GetRuntimeCollection();
        LevelObject* Spawn(const Assets::AssetReference<LevelObjectVariantAsset>& variant, const Math::Vector3& position, const Math::Quaternion& rotation, LevelObject* parent = nullptr);

        template <typename T>
        T* Spawn(const Assets::AssetReference<LevelObjectVariantAsset>& variant, const Math::Vector3& position, const Math::Quaternion& rotation, LevelObject* parent = nullptr)
        {
            return Reflection::Cast<T>(Spawn(variant, position, rotation, parent));
        }

        const std::string& GetName() const { return m_name; }
        void SetName(const std::string& name) { m_name = name; }
        const Core::Guid& GetAsset() const { return m_asset; }
        void SetAsset(const Core::Guid& asset) { m_asset = asset; }

        LevelCollection* AddCollection(const std::string& name, const Core::Guid& asset = {});
        void RemoveCollection(LevelCollection* collection);
        LevelCollection* FindCollection(const Core::Guid& asset) const;
        LevelCollection* FindCollectionByName(const std::string& name) const;
        LevelCollection* GetDefaultCollection();
        const std::vector<std::unique_ptr<LevelCollection>>& GetCollections() const { return m_collections; }

        LevelObject* CreateObject(const Reflection::TypeInfo& type, LevelCollection* collection = nullptr, LevelObject* parent = nullptr, const Core::Guid& id = {}, bool initialize = true);
        LevelObject* AdoptObject(std::unique_ptr<LevelObject> object, LevelCollection* collection, LevelObject* parent, bool initialize);
        void InitializeObject(LevelObject& object);
        void InitializeHierarchy(LevelObject& root);

        template <typename T>
        T* CreateObject(LevelCollection* collection = nullptr, LevelObject* parent = nullptr)
        {
            return static_cast<T*>(CreateObject(T::StaticType(), collection, parent));
        }

        void DestroyObject(LevelObject* object);
        void DestroyObjectImmediate(LevelObject* object);
        void RemoveComponent(LevelObjectComponent& component);

        LevelObject* FindObject(const Core::Guid& id) const;
        LevelObject* FindObjectByName(const std::string& name) const;
        const std::vector<std::unique_ptr<LevelObject>>& GetObjects() const { return m_objects; }
        size_t GetObjectCount() const { return m_objects.size(); }

        template <typename T>
        std::vector<T*> FindObjectsOfType() const
        {
            std::vector<T*> result;
            for (const auto& object : m_objects)
            {
                if (!object->IsPendingDestroy() && object->IsA(T::StaticType()))
                {
                    result.push_back(static_cast<T*>(object.get()));
                }
            }
            return result;
        }

        template <typename T>
        T* FindObjectOfType() const
        {
            for (const auto& object : m_objects)
            {
                if (!object->IsPendingDestroy() && object->IsA(T::StaticType()))
                {
                    return static_cast<T*>(object.get());
                }
            }
            return nullptr;
        }

        template <typename T>
        std::vector<T*> FindComponentsOfType(bool includeInactive = false) const
        {
            std::vector<T*> result;
            for (const auto& object : m_objects)
            {
                if (object->IsPendingDestroy() || (!includeInactive && !object->IsActiveInHierarchy()))
                {
                    continue;
                }
                if constexpr (std::is_same_v<T, Transform>)
                {
                    result.push_back(&object->GetTransform());
                }
                else
                {
                    for (const auto& component : object->GetComponents())
                    {
                        if (component->IsA(T::StaticType()) && !component->IsPendingDestroy() && (includeInactive || component->IsEnabled()))
                        {
                            result.push_back(static_cast<T*>(component.get()));
                        }
                    }
                }
            }
            return result;
        }

        void ForEachObject(const std::function<void(LevelObject&)>& visitor) const;

        bool IsPlaying() const { return m_isPlaying; }
        void BeginPlay();
        void Update(float deltaTime);
        void EndPlay();

        bool IsPaused() const { return m_isPaused; }
        void SetPaused(bool paused) { m_isPaused = paused; }
        void Step(float deltaTime);
        float GetTime() const { return m_time; }
        float GetUnscaledTime() const { return m_unscaledTime; }
        float GetDeltaTime() const { return m_deltaTime; }
        float GetUnscaledDeltaTime() const { return m_unscaledDeltaTime; }
        float GetFixedDeltaTime() const { return m_fixedDeltaTime; }
        void SetFixedDeltaTime(float fixedDeltaTime) { m_fixedDeltaTime = fixedDeltaTime > 0.0f ? fixedDeltaTime : m_fixedDeltaTime; }
        float GetTimeScale() const { return m_timeScale; }
        void SetTimeScale(float timeScale) { m_timeScale = timeScale < 0.0f ? 0.0f : timeScale; }
        float GetFixedInterpolation() const { return m_fixedAccumulator / m_fixedDeltaTime; }
        uint64_t GetFrameCount() const { return m_frameCount; }

        template <typename T>
        T* GetSubsystem() const
        {
            for (const auto& subsystem : m_subsystems)
            {
                if (T* typed = Reflection::Cast<T>(subsystem.get()))
                {
                    return typed;
                }
            }
            return nullptr;
        }

        const std::vector<std::unique_ptr<LevelSubsystem>>& GetSubsystems() const { return m_subsystems; }
        void CreateSubsystems();

        uint64_t GetHierarchyRevision() const { return m_hierarchyRevision; }
        void MarkHierarchyChanged() { ++m_hierarchyRevision; }

        void RefreshEnabledState(LevelObject& object);
        void OnComponentAttached(LevelObjectComponent& component);
        void MoveToCollection(LevelObject& object, LevelCollection* collection);
        void SetRootCollection(LevelObject& root, LevelCollection* collection, int siblingIndex = -1);

        Reflection::EngineObject* CreateEngineObject(const Reflection::TypeInfo& type);
        void DestroyEngineObject(Reflection::EngineObject* object);
        const std::vector<std::unique_ptr<Reflection::EngineObject>>& GetEngineObjects() const { return m_engineObjects; }

        template <typename T>
        T* FindEngineObjectOfType() const
        {
            for (const auto& object : m_engineObjects)
            {
                T* typed = Reflection::Cast<T>(object.get());
                if (typed && !typed->IsPendingDestroy())
                {
                    return typed;
                }
            }
            return nullptr;
        }

        template <typename T>
        T* CreateEngineObject()
        {
            return Reflection::Cast<T>(CreateEngineObject(T::StaticType()));
        }

        static LevelObjectRuntimeState GetRuntimeState(const LevelObject& object);
        static void SetRuntimeState(LevelObject& object, const LevelObjectRuntimeState& state);
        static ComponentRuntimeState GetRuntimeState(const LevelObjectComponent& component);
        static void SetRuntimeState(LevelObjectComponent& component, const ComponentRuntimeState& state);
        static Reflection::EngineObjectState GetRuntimeState(const Reflection::EngineObject& object);
        static void SetRuntimeState(Reflection::EngineObject& object, const Reflection::EngineObjectState& state);
        static void AttachComponent(LevelObject& object, std::unique_ptr<LevelObjectComponent> component, int index = -1);

        std::unique_ptr<LevelObject> ReleaseObjectForReplacement(LevelObject& object, std::vector<LevelObject*>& children, std::vector<std::unique_ptr<LevelObjectComponent>>& components);
        void InstallReplacement(std::unique_ptr<LevelObject> replacement, const std::vector<LevelObject*>& children, LevelObject* parent, LevelCollection* collection, int siblingIndex);
        std::unique_ptr<LevelObjectComponent> ReleaseComponentForReplacement(LevelObject& object, size_t index);
        void InstallComponentReplacement(LevelObject& object, std::unique_ptr<LevelObjectComponent> component, size_t index);
        std::unique_ptr<Reflection::EngineObject> ReleaseEngineObjectForReplacement(size_t index);
        void InstallEngineObjectReplacement(std::unique_ptr<Reflection::EngineObject> replacement, size_t index);
        void RemapReferences(const Reflection::ReferenceRemap& remap, const std::unordered_set<const Reflection::EngineObject*>& skip);
        void RemoveEmptyEngineObjects();

        Core::Event<LevelObject&> OnObjectCreated;
        Core::Event<LevelObject&> OnObjectDestroyed;

        Serialization::JsonDocument& GetSettings() { return m_settings; }
        const Serialization::JsonDocument& GetSettings() const { return m_settings; }

        void* GetUserData() const { return m_userData; }
        void SetUserData(void* userData) { m_userData = userData; }

    private:
        void ActivateForPlay(LevelObject& object);
        void ApplyEnabledState(LevelObject& object, bool notify);
        void LoadComponent(LevelObjectComponent& component);
        void RunPendingStarts();
        void FlushDestroyed();
        void DestroyRecursive(LevelObject& object, bool callLifecycle, std::vector<LevelObject*>& order);
        void FinalizeDestroy(const std::vector<LevelObject*>& order);
        void Tick(float deltaTime, bool stepping);
        void DispatchUpdate(void (Reflection::EngineObject::*callback)(float), float deltaTime);
        void LoadEngineObject(Reflection::EngineObject& object);
        static bool IsUpdatable(const Reflection::EngineObject& object);

        std::string m_name;
        Core::Guid m_asset;
        std::vector<std::unique_ptr<LevelCollection>> m_collections;
        std::vector<std::unique_ptr<LevelObject>> m_objects;
        std::unordered_map<Core::Guid, LevelObject*> m_index;
        std::vector<std::unique_ptr<LevelSubsystem>> m_subsystems;
        std::vector<std::unique_ptr<Reflection::EngineObject>> m_engineObjects;
        std::vector<LevelObject*> m_pendingDestroy;
        std::vector<LevelObjectComponent*> m_pendingComponentDestroy;
        std::unordered_set<Core::Guid> m_replacedPendingDestroy;
        bool m_isPlaying = false;
        bool m_isPaused = false;
        bool m_isEnding = false;
        float m_time = 0.0f;
        float m_unscaledTime = 0.0f;
        float m_deltaTime = 0.0f;
        float m_unscaledDeltaTime = 0.0f;
        float m_fixedDeltaTime = 1.0f / 60.0f;
        float m_fixedAccumulator = 0.0f;
        float m_timeScale = 1.0f;
        uint64_t m_frameCount = 0;
        uint64_t m_hierarchyRevision = 0;
        void* m_userData = nullptr;
        Serialization::JsonDocument m_settings;
    };
}
