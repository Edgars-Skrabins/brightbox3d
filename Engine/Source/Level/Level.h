#pragma once

#include "Core/Event.h"
#include "Core/Guid.h"
#include "Level/LevelCollection.h"
#include "Level/LevelObjectInstance.h"
#include "Level/LevelSubsystem.h"
#include "Serialization/Json.h"

#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::Level
{
    class Level
    {
    public:
        explicit Level(std::string name = "Level");
        ~Level();

        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;

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

        LevelObjectInstance* CreateObject(const Reflection::TypeInfo& type, LevelCollection* collection = nullptr, LevelObjectInstance* parent = nullptr, const Core::Guid& id = {}, bool initialize = true);
        LevelObjectInstance* AdoptObject(std::unique_ptr<LevelObjectInstance> object, LevelCollection* collection, LevelObjectInstance* parent, bool initialize);
        void InitializeObject(LevelObjectInstance& object);

        template <typename T>
        T* CreateObject(LevelCollection* collection = nullptr, LevelObjectInstance* parent = nullptr)
        {
            return static_cast<T*>(CreateObject(T::StaticType(), collection, parent));
        }

        void DestroyObject(LevelObjectInstance* object);
        void DestroyObjectImmediate(LevelObjectInstance* object);
        void RemoveComponent(Component& component);

        LevelObjectInstance* FindObject(const Core::Guid& id) const;
        LevelObjectInstance* FindObjectByName(const std::string& name) const;
        const std::vector<std::unique_ptr<LevelObjectInstance>>& GetObjects() const { return m_objects; }
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

        void ForEachObject(const std::function<void(LevelObjectInstance&)>& visitor) const;

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
                if (auto* typed = dynamic_cast<T*>(subsystem.get()))
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

        void RefreshEnabledState(LevelObjectInstance& object);
        void OnComponentAttached(Component& component);
        void MoveToCollection(LevelObjectInstance& object, LevelCollection* collection);
        void SetRootCollection(LevelObjectInstance& root, LevelCollection* collection, int siblingIndex = -1);

        std::unique_ptr<LevelObjectInstance> ReleaseObjectForReplacement(LevelObjectInstance& object);
        void InstallReplacement(std::unique_ptr<LevelObjectInstance> replacement, LevelObjectInstance* parent, LevelCollection* collection, int siblingIndex);

        Core::Event<LevelObjectInstance&> OnObjectCreated;
        Core::Event<LevelObjectInstance&> OnObjectDestroyed;

        Serialization::JsonDocument& GetSettings() { return m_settings; }
        const Serialization::JsonDocument& GetSettings() const { return m_settings; }

        void* GetUserData() const { return m_userData; }
        void SetUserData(void* userData) { m_userData = userData; }

    private:
        void ActivateForPlay(LevelObjectInstance& object);
        void ApplyEnabledState(LevelObjectInstance& object, bool notify);
        void LoadComponent(Component& component);
        void RunPendingStarts();
        void FlushDestroyed();
        void DestroyRecursive(LevelObjectInstance& object, bool callLifecycle, std::vector<LevelObjectInstance*>& order);
        void FinalizeDestroy(const std::vector<LevelObjectInstance*>& order);
        void Tick(float deltaTime, bool stepping);

        std::string m_name;
        Core::Guid m_asset;
        std::vector<std::unique_ptr<LevelCollection>> m_collections;
        std::vector<std::unique_ptr<LevelObjectInstance>> m_objects;
        std::unordered_map<Core::Guid, LevelObjectInstance*> m_index;
        std::vector<std::unique_ptr<LevelSubsystem>> m_subsystems;
        std::vector<LevelObjectInstance*> m_pendingDestroy;
        std::vector<Component*> m_pendingComponentDestroy;
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
