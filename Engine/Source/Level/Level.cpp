#include "Level/Level.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Level/LevelSerializer.h"

#include <algorithm>
#include <mutex>

namespace BrightBox3D::Level
{
    namespace
    {
        constexpr int MaxFixedStepsPerFrame = 8;
        constexpr float MaxTickDeltaTime = 0.25f;

        std::mutex& LevelsMutex()
        {
            static std::mutex* mutex = new std::mutex();
            return *mutex;
        }

        std::vector<Level*>& LiveLevels()
        {
            static std::vector<Level*>* levels = new std::vector<Level*>();
            return *levels;
        }
    }

    std::vector<Level*> Level::GetAllLevels()
    {
        std::lock_guard<std::mutex> lock(LevelsMutex());
        return LiveLevels();
    }

    Level::Level(std::string name) : m_name(std::move(name))
    {
        m_settings.SetObject();
        CreateSubsystems();
        std::lock_guard<std::mutex> lock(LevelsMutex());
        LiveLevels().push_back(this);
    }

    Level::~Level()
    {
        {
            std::lock_guard<std::mutex> lock(LevelsMutex());
            auto& levels = LiveLevels();
            levels.erase(std::remove(levels.begin(), levels.end(), this), levels.end());
        }
        if (m_isPlaying)
        {
            EndPlay();
        }
        for (auto& object : m_objects)
        {
            for (const auto& component : object->GetComponents())
            {
                for (auto& subsystem : m_subsystems)
                {
                    subsystem->OnComponentRemoved(*component);
                }
            }
        }
        m_index.clear();
        m_objects.clear();
        m_subsystems.clear();
        m_collections.clear();
    }

    void Level::CreateSubsystems()
    {
        m_subsystems.clear();
        for (auto& [name, factory] : LevelSubsystemRegistry::GetAll())
        {
            if (auto subsystem = factory(*this))
            {
                m_subsystems.push_back(std::move(subsystem));
            }
        }
    }

    LevelCollection* Level::AddCollection(const std::string& name, const Core::Guid& asset)
    {
        m_collections.push_back(std::make_unique<LevelCollection>(*this, name, asset));
        MarkHierarchyChanged();
        return m_collections.back().get();
    }

    void Level::RemoveCollection(LevelCollection* collection)
    {
        if (!collection)
        {
            return;
        }
        std::vector<LevelObject*> roots = collection->GetRoots();
        for (LevelObject* root : roots)
        {
            DestroyObjectImmediate(root);
        }
        m_collections.erase(std::remove_if(m_collections.begin(), m_collections.end(), [&](const auto& entry) { return entry.get() == collection; }), m_collections.end());
        MarkHierarchyChanged();
    }

    LevelCollection* Level::FindCollection(const Core::Guid& asset) const
    {
        for (const auto& collection : m_collections)
        {
            if (collection->GetAsset() == asset)
            {
                return collection.get();
            }
        }
        return nullptr;
    }

    LevelCollection* Level::FindCollectionByName(const std::string& name) const
    {
        for (const auto& collection : m_collections)
        {
            if (collection->GetName() == name)
            {
                return collection.get();
            }
        }
        return nullptr;
    }

    LevelCollection* Level::GetDefaultCollection()
    {
        if (m_collections.empty())
        {
            AddCollection("Main");
        }
        return m_collections.front().get();
    }

    LevelObject* Level::CreateObject(const Reflection::TypeInfo& type, LevelCollection* collection, LevelObject* parent, const Core::Guid& id, bool initialize)
    {
        if (!type.IsA(LevelObject::StaticType()))
        {
            Core::Log::Error(Core::Text(Core::EngineText::NotLevelObjectType, type.GetName(), LevelObject::StaticType().GetName()));
            return nullptr;
        }
        std::unique_ptr<Reflection::EngineObject> created(type.CreateInstance());
        if (!created)
        {
            Core::Log::Error(Core::Text(Core::EngineText::TypeAbstract, type.GetName()));
            return nullptr;
        }
        std::unique_ptr<LevelObject> object(static_cast<LevelObject*>(created.release()));
        if (id.IsValid())
        {
            object->m_state.id = id;
        }
        object->m_state.name = type.GetName();
        return AdoptObject(std::move(object), collection, parent, initialize);
    }

    LevelObject* Level::AdoptObject(std::unique_ptr<LevelObject> object, LevelCollection* collection, LevelObject* parent, bool initialize)
    {
        if (m_index.count(object->GetId()))
        {
            Core::Log::Warning(Core::Text(Core::EngineText::ObjectIdCollision, object->GetName()));
            object->m_state.id = Core::Guid::Generate();
        }
        if (parent && parent->m_level != this)
        {
            parent = nullptr;
        }
        if (parent)
        {
            collection = parent->m_collection;
        }
        if (!collection)
        {
            collection = GetDefaultCollection();
        }

        LevelObject* raw = object.get();
        raw->m_level = this;
        raw->m_collection = collection;
        raw->m_parent = parent;
        raw->m_transform.m_owner = raw;
        if (parent)
        {
            parent->m_children.push_back(raw);
        }
        else
        {
            collection->AddRoot(raw);
        }
        raw->m_transform.MarkDirty();
        m_index[raw->GetId()] = raw;
        m_objects.push_back(std::move(object));

        for (const auto& component : raw->m_components)
        {
            component->m_owner = raw;
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnComponentAdded(*component);
            }
        }

        MarkHierarchyChanged();
        OnObjectCreated.Broadcast(*raw);
        if (initialize)
        {
            InitializeObject(*raw);
        }
        return raw;
    }

    void Level::InitializeObject(LevelObject& object)
    {
        object.OnInitialize();
        for (const auto& component : object.m_components)
        {
            component->OnInitialize();
        }
        if (m_isPlaying)
        {
            ActivateForPlay(object);
        }
    }

    void Level::ActivateForPlay(LevelObject& object)
    {
        if (!object.IsActiveInHierarchy() || object.m_engineState.isPendingDestroy)
        {
            return;
        }
        if (!object.m_engineState.hasLoaded)
        {
            object.m_engineState.hasLoaded = true;
            object.OnGameLoad();
        }
        for (size_t i = 0; i < object.m_components.size(); ++i)
        {
            LoadComponent(*object.m_components[i]);
        }
        ApplyEnabledState(object, true);
    }

    void Level::LoadComponent(LevelObjectComponent& component)
    {
        if (component.m_engineState.hasLoaded || component.m_engineState.isPendingDestroy)
        {
            return;
        }
        component.m_engineState.hasLoaded = true;
        component.OnGameLoad();
    }

    void Level::ApplyEnabledState(LevelObject& object, bool notify)
    {
        bool objectEnabled = object.IsActiveInHierarchy() && !object.m_engineState.isPendingDestroy;
        if (objectEnabled && !object.m_engineState.hasLoaded && m_isPlaying)
        {
            object.m_engineState.hasLoaded = true;
            object.OnGameLoad();
        }
        if (objectEnabled != object.m_engineState.isEnabledState)
        {
            object.m_engineState.isEnabledState = objectEnabled;
            if (notify && m_isPlaying)
            {
                if (objectEnabled)
                {
                    object.OnEnable();
                }
                else
                {
                    object.OnDisable();
                }
            }
        }
        for (size_t i = 0; i < object.m_components.size(); ++i)
        {
            LevelObjectComponent& component = *object.m_components[i];
            bool componentEnabled = objectEnabled && component.m_state.isEnabled && !component.m_engineState.isPendingDestroy;
            if (componentEnabled && m_isPlaying)
            {
                LoadComponent(component);
            }
            if (componentEnabled != component.m_engineState.isEnabledState)
            {
                component.m_engineState.isEnabledState = componentEnabled;
                if (notify && m_isPlaying)
                {
                    if (componentEnabled)
                    {
                        component.OnEnable();
                    }
                    else
                    {
                        component.OnDisable();
                    }
                }
            }
        }
    }

    void Level::RefreshEnabledState(LevelObject& object)
    {
        ApplyEnabledState(object, true);
        std::vector<LevelObject*> children = object.m_children;
        for (LevelObject* child : children)
        {
            RefreshEnabledState(*child);
        }
    }

    void Level::OnComponentAttached(LevelObjectComponent& component)
    {
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnComponentAdded(component);
        }
        component.OnInitialize();
        if (m_isPlaying && component.m_owner)
        {
            ApplyEnabledState(*component.m_owner, true);
        }
    }

    void Level::RemoveComponent(LevelObjectComponent& component)
    {
        if (component.m_engineState.isPendingDestroy)
        {
            return;
        }
        if (m_isPlaying && !m_isEnding)
        {
            component.m_engineState.isPendingDestroy = true;
            m_pendingComponentDestroy.push_back(&component);
            return;
        }
        LevelObject* owner = component.m_owner;
        if (m_isPlaying)
        {
            if (component.m_engineState.isEnabledState)
            {
                component.m_engineState.isEnabledState = false;
                component.OnDisable();
            }
            component.OnDestroy();
        }
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnComponentRemoved(component);
        }
        owner->DetachComponent(&component);
    }

    void Level::MoveToCollection(LevelObject& object, LevelCollection* collection)
    {
        object.m_collection = collection;
        for (LevelObject* child : object.m_children)
        {
            MoveToCollection(*child, collection);
        }
    }

    void Level::SetRootCollection(LevelObject& root, LevelCollection* collection, int siblingIndex)
    {
        if (!collection || root.m_parent)
        {
            return;
        }
        if (root.m_collection)
        {
            root.m_collection->RemoveRoot(&root);
        }
        MoveToCollection(root, collection);
        collection->AddRoot(&root, siblingIndex);
        MarkHierarchyChanged();
    }

    void Level::DestroyObject(LevelObject* object)
    {
        if (!object || object->m_level != this || object->m_engineState.isPendingDestroy)
        {
            return;
        }
        if (!m_isPlaying || m_isEnding)
        {
            DestroyObjectImmediate(object);
            return;
        }
        std::function<void(LevelObject&)> mark = [&](LevelObject& target)
        {
            target.m_engineState.isPendingDestroy = true;
            for (LevelObject* child : target.m_children)
            {
                mark(*child);
            }
        };
        mark(*object);
        m_pendingDestroy.push_back(object);
    }

    void Level::DestroyRecursive(LevelObject& object, bool callLifecycle, std::vector<LevelObject*>& order)
    {
        object.m_engineState.isPendingDestroy = true;
        std::vector<LevelObject*> children = object.m_children;
        for (LevelObject* child : children)
        {
            DestroyRecursive(*child, callLifecycle, order);
        }
        if (callLifecycle)
        {
            for (const auto& component : object.m_components)
            {
                if (component->m_engineState.isEnabledState)
                {
                    component->m_engineState.isEnabledState = false;
                    component->OnDisable();
                }
            }
            if (object.m_engineState.isEnabledState)
            {
                object.m_engineState.isEnabledState = false;
                object.OnDisable();
            }
            for (const auto& component : object.m_components)
            {
                component->OnDestroy();
            }
            object.OnDestroy();
        }
        order.push_back(&object);
    }

    void Level::FinalizeDestroy(const std::vector<LevelObject*>& order)
    {
        for (LevelObject* object : order)
        {
            OnObjectDestroyed.Broadcast(*object);
            for (const auto& component : object->m_components)
            {
                for (auto& subsystem : m_subsystems)
                {
                    subsystem->OnComponentRemoved(*component);
                }
            }
            if (object->m_parent)
            {
                auto& siblings = object->m_parent->m_children;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), object), siblings.end());
            }
            else if (object->m_collection)
            {
                object->m_collection->RemoveRoot(object);
            }
            object->m_children.clear();
            m_index.erase(object->GetId());
        }
        std::unordered_map<LevelObject*, bool> doomed;
        for (LevelObject* object : order)
        {
            doomed[object] = true;
        }
        m_pendingDestroy.erase(std::remove_if(m_pendingDestroy.begin(), m_pendingDestroy.end(), [&](LevelObject* object) { return doomed.count(object) > 0; }), m_pendingDestroy.end());
        m_pendingComponentDestroy.erase(std::remove_if(m_pendingComponentDestroy.begin(), m_pendingComponentDestroy.end(), [&](LevelObjectComponent* component) { return doomed.count(component->GetOwner()) > 0; }), m_pendingComponentDestroy.end());
        m_objects.erase(std::remove_if(m_objects.begin(), m_objects.end(), [&](const auto& entry) { return doomed.count(entry.get()) > 0; }), m_objects.end());
        MarkHierarchyChanged();
    }

    void Level::DestroyObjectImmediate(LevelObject* object)
    {
        if (!object || object->m_level != this)
        {
            return;
        }
        std::vector<LevelObject*> order;
        DestroyRecursive(*object, m_isPlaying, order);
        FinalizeDestroy(order);
    }

    void Level::FlushDestroyed()
    {
        for (auto it = m_engineObjects.begin(); it != m_engineObjects.end();)
        {
            if (*it && (*it)->m_engineState.isPendingDestroy)
            {
                std::unique_ptr<Reflection::EngineObject> doomed = std::move(*it);
                it = m_engineObjects.erase(it);
                if (doomed->m_engineState.hasLoaded)
                {
                    doomed->OnGameEnd();
                }
                doomed->OnDestroy();
            }
            else
            {
                ++it;
            }
        }
        if (!m_pendingComponentDestroy.empty())
        {
            std::vector<LevelObjectComponent*> components;
            components.swap(m_pendingComponentDestroy);
            for (LevelObjectComponent* component : components)
            {
                LevelObject* owner = component->m_owner;
                if (!owner || owner->m_engineState.isPendingDestroy)
                {
                    continue;
                }
                if (component->m_engineState.isEnabledState)
                {
                    component->m_engineState.isEnabledState = false;
                    component->OnDisable();
                }
                component->OnDestroy();
                for (auto& subsystem : m_subsystems)
                {
                    subsystem->OnComponentRemoved(*component);
                }
                owner->DetachComponent(component);
            }
        }
        if (!m_pendingDestroy.empty())
        {
            std::vector<LevelObject*> roots = m_pendingDestroy;
            std::vector<LevelObject*> order;
            for (LevelObject* root : roots)
            {
                bool nested = false;
                for (LevelObject* other : roots)
                {
                    if (other != root && root->IsDescendantOf(other))
                    {
                        nested = true;
                        break;
                    }
                }
                if (!nested)
                {
                    DestroyRecursive(*root, true, order);
                }
            }
            FinalizeDestroy(order);
        }
    }

    LevelObject* Level::FindObject(const Core::Guid& id) const
    {
        auto it = m_index.find(id);
        return it == m_index.end() ? nullptr : it->second;
    }

    LevelObject* Level::FindObjectByName(const std::string& name) const
    {
        for (const auto& object : m_objects)
        {
            if (object->GetName() == name && !object->IsPendingDestroy())
            {
                return object.get();
            }
        }
        return nullptr;
    }

    void Level::ForEachObject(const std::function<void(LevelObject&)>& visitor) const
    {
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            visitor(*m_objects[i]);
        }
    }

    void Level::BeginPlay()
    {
        if (m_isPlaying)
        {
            return;
        }
        m_isPlaying = true;
        m_time = 0.0f;
        m_unscaledTime = 0.0f;
        m_fixedAccumulator = 0.0f;
        m_frameCount = 0;
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnGameStart();
        }
        size_t count = m_objects.size();
        for (size_t i = 0; i < count && i < m_objects.size(); ++i)
        {
            ActivateForPlay(*m_objects[i]);
        }
        size_t engineCount = m_engineObjects.size();
        for (size_t i = 0; i < engineCount && i < m_engineObjects.size(); ++i)
        {
            LoadEngineObject(*m_engineObjects[i]);
        }
    }

    void Level::LoadEngineObject(Reflection::EngineObject& object)
    {
        Reflection::EngineObjectState& state = object.m_engineState;
        if (state.hasLoaded || state.isPendingDestroy)
        {
            return;
        }
        state.hasLoaded = true;
        state.isEnabledState = true;
        object.OnGameLoad();
    }

    void Level::RunPendingStarts()
    {
        size_t count = m_objects.size();
        for (size_t i = 0; i < count && i < m_objects.size(); ++i)
        {
            LevelObject& object = *m_objects[i];
            if (object.m_engineState.isPendingDestroy || !object.m_engineState.isEnabledState)
            {
                continue;
            }
            if (!object.m_engineState.hasStarted)
            {
                object.m_engineState.hasStarted = true;
                object.OnGameStart();
            }
            for (size_t c = 0; c < object.m_components.size(); ++c)
            {
                LevelObjectComponent& component = *object.m_components[c];
                if (component.m_engineState.isEnabledState && !component.m_engineState.hasStarted && !component.m_engineState.isPendingDestroy)
                {
                    component.m_engineState.hasStarted = true;
                    component.OnGameStart();
                }
            }
        }
        size_t engineCount = m_engineObjects.size();
        for (size_t i = 0; i < engineCount && i < m_engineObjects.size(); ++i)
        {
            Reflection::EngineObject* object = m_engineObjects[i].get();
            if (object && object->m_engineState.isEnabledState && !object->m_engineState.hasStarted && !object->m_engineState.isPendingDestroy)
            {
                object->m_engineState.hasStarted = true;
                object->OnGameStart();
            }
        }
    }

    void Level::Update(float deltaTime)
    {
        if (!m_isPlaying)
        {
            return;
        }
        if (m_isPaused)
        {
            m_unscaledDeltaTime = deltaTime;
            m_deltaTime = 0.0f;
            FlushDestroyed();
            return;
        }
        Tick(deltaTime, false);
    }

    void Level::Step(float deltaTime)
    {
        if (m_isPlaying)
        {
            Tick(deltaTime, true);
        }
    }

    void Level::Tick(float deltaTime, bool stepping)
    {
        deltaTime = std::clamp(deltaTime, 0.0f, MaxTickDeltaTime);
        m_unscaledDeltaTime = deltaTime;
        m_deltaTime = deltaTime * m_timeScale;
        m_unscaledTime += deltaTime;
        m_time += m_deltaTime;
        ++m_frameCount;

        RunPendingStarts();

        m_fixedAccumulator += m_deltaTime;
        if (stepping && m_fixedAccumulator < m_fixedDeltaTime)
        {
            m_fixedAccumulator = m_fixedDeltaTime;
        }
        int steps = 0;
        while (m_fixedAccumulator >= m_fixedDeltaTime && steps < MaxFixedStepsPerFrame)
        {
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnPreFixedUpdate(m_fixedDeltaTime);
            }
            DispatchUpdate(&Reflection::EngineObject::OnGameUpdateFixed, m_fixedDeltaTime);
            m_fixedAccumulator -= m_fixedDeltaTime;
            ++steps;
        }
        if (steps == MaxFixedStepsPerFrame)
        {
            m_fixedAccumulator = std::min(m_fixedAccumulator, m_fixedDeltaTime);
        }

        DispatchUpdate(&Reflection::EngineObject::OnGameUpdate, m_deltaTime);
        DispatchUpdate(&Reflection::EngineObject::OnGameUpdateLate, m_deltaTime);
        FlushDestroyed();
    }

    void Level::DispatchUpdate(void (Reflection::EngineObject::*callback)(float), float deltaTime)
    {
        size_t count = m_objects.size();
        for (size_t i = 0; i < count && i < m_objects.size(); ++i)
        {
            LevelObject& object = *m_objects[i];
            if (!IsUpdatable(object))
            {
                continue;
            }
            (object.*callback)(deltaTime);
            for (size_t c = 0; c < object.m_components.size(); ++c)
            {
                LevelObjectComponent& component = *object.m_components[c];
                if (IsUpdatable(component))
                {
                    (component.*callback)(deltaTime);
                }
            }
        }
        size_t engineCount = m_engineObjects.size();
        for (size_t i = 0; i < engineCount && i < m_engineObjects.size(); ++i)
        {
            Reflection::EngineObject* object = m_engineObjects[i].get();
            if (object && IsUpdatable(*object))
            {
                (object->*callback)(deltaTime);
            }
        }
        for (auto& subsystem : m_subsystems)
        {
            (subsystem.get()->*callback)(deltaTime);
        }
    }

    bool Level::IsUpdatable(const Reflection::EngineObject& object)
    {
        const Reflection::EngineObjectState& state = object.m_engineState;
        return state.isEnabledState && state.hasStarted && !state.isPendingDestroy;
    }

    void Level::EndPlay()
    {
        if (!m_isPlaying)
        {
            return;
        }
        FlushDestroyed();
        m_isEnding = true;
        for (auto it = m_engineObjects.rbegin(); it != m_engineObjects.rend(); ++it)
        {
            Reflection::EngineObject& object = **it;
            if (object.m_engineState.hasLoaded)
            {
                object.m_engineState.isEnabledState = false;
                object.OnGameEnd();
            }
        }
        for (auto it = m_engineObjects.rbegin(); it != m_engineObjects.rend(); ++it)
        {
            Reflection::EngineObject& object = **it;
            if (object.m_engineState.hasLoaded)
            {
                object.OnDestroy();
            }
        }
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            LevelObject& object = *m_objects[i];
            for (const auto& component : object.m_components)
            {
                if (component->m_engineState.isEnabledState)
                {
                    component->m_engineState.isEnabledState = false;
                    component->OnDisable();
                }
            }
            if (object.m_engineState.isEnabledState)
            {
                object.m_engineState.isEnabledState = false;
                object.OnDisable();
            }
        }
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            LevelObject& object = *m_objects[i];
            if (!object.m_engineState.hasLoaded)
            {
                continue;
            }
            object.OnGameEnd();
            for (const auto& component : object.m_components)
            {
                if (component->m_engineState.hasLoaded)
                {
                    component->OnGameEnd();
                }
            }
        }
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            LevelObject& object = *m_objects[i];
            for (const auto& component : object.m_components)
            {
                if (component->m_engineState.hasLoaded)
                {
                    component->OnDestroy();
                }
            }
            if (object.m_engineState.hasLoaded)
            {
                object.OnDestroy();
            }
        }
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnGameEnd();
        }
        m_isEnding = false;
        m_isPlaying = false;
    }

    LevelObjectRuntimeState Level::GetRuntimeState(const LevelObject& object)
    {
        return { object.m_engineState, object.m_state };
    }

    void Level::SetRuntimeState(LevelObject& object, const LevelObjectRuntimeState& state)
    {
        object.m_engineState = state.engine;
        object.m_state = state.object;
    }

    ComponentRuntimeState Level::GetRuntimeState(const LevelObjectComponent& component)
    {
        return { component.m_engineState, component.m_state };
    }

    void Level::SetRuntimeState(LevelObjectComponent& component, const ComponentRuntimeState& state)
    {
        component.m_engineState = state.engine;
        component.m_state = state.component;
    }

    Reflection::EngineObjectState Level::GetRuntimeState(const Reflection::EngineObject& object)
    {
        return object.m_engineState;
    }

    void Level::SetRuntimeState(Reflection::EngineObject& object, const Reflection::EngineObjectState& state)
    {
        object.m_engineState = state;
    }

    void Level::AttachComponent(LevelObject& object, std::unique_ptr<LevelObjectComponent> component, int index)
    {
        object.AttachComponent(std::move(component), index);
    }

    std::unique_ptr<LevelObject> Level::ReleaseObjectForReplacement(LevelObject& object, std::vector<LevelObject*>& children, std::vector<std::unique_ptr<LevelObjectComponent>>& components)
    {
        for (const auto& component : object.m_components)
        {
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnComponentRemoved(*component);
            }
        }
        auto queued = std::find(m_pendingDestroy.begin(), m_pendingDestroy.end(), &object);
        if (queued != m_pendingDestroy.end())
        {
            m_pendingDestroy.erase(queued);
            m_replacedPendingDestroy.insert(object.GetId());
        }
        m_pendingComponentDestroy.erase(std::remove_if(m_pendingComponentDestroy.begin(), m_pendingComponentDestroy.end(), [&](LevelObjectComponent* component) { return component->m_owner == &object; }), m_pendingComponentDestroy.end());
        if (object.m_parent)
        {
            auto& siblings = object.m_parent->m_children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), &object), siblings.end());
        }
        else if (object.m_collection)
        {
            object.m_collection->RemoveRoot(&object);
        }
        children = std::move(object.m_children);
        object.m_children.clear();
        components = std::move(object.m_components);
        object.m_components.clear();
        m_index.erase(object.GetId());
        for (auto it = m_objects.begin(); it != m_objects.end(); ++it)
        {
            if (it->get() == &object)
            {
                std::unique_ptr<LevelObject> released = std::move(*it);
                m_objects.erase(it);
                return released;
            }
        }
        return nullptr;
    }

    void Level::InstallReplacement(std::unique_ptr<LevelObject> replacement, const std::vector<LevelObject*>& children, LevelObject* parent, LevelCollection* collection, int siblingIndex)
    {
        LevelObject* raw = replacement.get();
        raw->m_level = this;
        raw->m_collection = collection;
        raw->m_parent = parent;
        raw->m_transform.m_owner = raw;
        if (parent)
        {
            auto& siblings = parent->m_children;
            if (siblingIndex < 0 || siblingIndex > static_cast<int>(siblings.size()))
            {
                siblings.push_back(raw);
            }
            else
            {
                siblings.insert(siblings.begin() + siblingIndex, raw);
            }
        }
        else if (collection)
        {
            collection->AddRoot(raw, siblingIndex);
        }
        for (LevelObject* child : children)
        {
            raw->m_children.push_back(child);
            child->m_parent = raw;
        }
        for (const auto& component : raw->m_components)
        {
            component->m_owner = raw;
            if (component->m_engineState.isPendingDestroy)
            {
                m_pendingComponentDestroy.push_back(component.get());
            }
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnComponentAdded(*component);
            }
        }
        if (m_replacedPendingDestroy.erase(raw->GetId()))
        {
            m_pendingDestroy.push_back(raw);
        }
        raw->m_transform.MarkDirty();
        m_index[raw->GetId()] = raw;
        m_objects.push_back(std::move(replacement));
        MarkHierarchyChanged();
    }

    std::unique_ptr<LevelObjectComponent> Level::ReleaseComponentForReplacement(LevelObject& object, size_t index)
    {
        LevelObjectComponent* component = object.m_components[index].get();
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnComponentRemoved(*component);
        }
        m_pendingComponentDestroy.erase(std::remove(m_pendingComponentDestroy.begin(), m_pendingComponentDestroy.end(), component), m_pendingComponentDestroy.end());
        std::unique_ptr<LevelObjectComponent> released = std::move(object.m_components[index]);
        object.m_components.erase(object.m_components.begin() + static_cast<std::ptrdiff_t>(index));
        return released;
    }

    void Level::InstallComponentReplacement(LevelObject& object, std::unique_ptr<LevelObjectComponent> component, size_t index)
    {
        LevelObjectComponent* raw = component.get();
        raw->m_owner = &object;
        index = std::min(index, object.m_components.size());
        object.m_components.insert(object.m_components.begin() + static_cast<std::ptrdiff_t>(index), std::move(component));
        if (raw->m_engineState.isPendingDestroy)
        {
            m_pendingComponentDestroy.push_back(raw);
        }
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnComponentAdded(*raw);
        }
        MarkHierarchyChanged();
    }

    Reflection::EngineObject* Level::CreateEngineObject(const Reflection::TypeInfo& type)
    {
        if (type.IsA(LevelObject::StaticType()) || type.IsA(LevelObjectComponent::StaticType()))
        {
            Core::Log::Error(Core::Text(Core::EngineText::WorldTypeAsEngineObject, type.GetName(), LevelObject::StaticType().GetName()));
            return nullptr;
        }
        std::unique_ptr<Reflection::EngineObject> object(type.CreateInstance());
        if (!object)
        {
            Core::Log::Error(Core::Text(Core::EngineText::TypeAbstract, type.GetName()));
            return nullptr;
        }
        Reflection::EngineObject* raw = object.get();
        raw->m_level = this;
        m_engineObjects.push_back(std::move(object));
        if (m_isPlaying)
        {
            LoadEngineObject(*raw);
        }
        return raw;
    }

    void Level::DestroyEngineObject(Reflection::EngineObject* object)
    {
        auto it = std::find_if(m_engineObjects.begin(), m_engineObjects.end(), [&](const auto& entry) { return entry.get() == object; });
        if (it == m_engineObjects.end() || object->m_engineState.isPendingDestroy)
        {
            return;
        }
        object->m_engineState.isPendingDestroy = true;
        if (!m_isPlaying)
        {
            std::unique_ptr<Reflection::EngineObject> doomed = std::move(*it);
            m_engineObjects.erase(it);
            doomed->OnDestroy();
        }
    }

    std::unique_ptr<Reflection::EngineObject> Level::ReleaseEngineObjectForReplacement(size_t index)
    {
        return std::move(m_engineObjects[index]);
    }

    void Level::InstallEngineObjectReplacement(std::unique_ptr<Reflection::EngineObject> replacement, size_t index)
    {
        if (replacement)
        {
            replacement->m_level = this;
        }
        m_engineObjects[index] = std::move(replacement);
    }

    void Level::RemapReferences(const Reflection::ReferenceRemap& remap, const std::unordered_set<const Reflection::EngineObject*>& skip)
    {
        Reflection::ReferenceList unused;
        auto remapObject = [&](Reflection::EngineObject& object)
        {
            if (!skip.count(&object))
            {
                Reflection::ReferenceCarrier carrier(Reflection::ReferenceCarrier::Mode::Remap, unused, remap);
                object.CarryReferences(carrier);
            }
        };
        for (const auto& object : m_engineObjects)
        {
            if (object)
            {
                remapObject(*object);
            }
        }
        for (const auto& object : m_objects)
        {
            remapObject(*object);
            remapObject(object->GetTransform());
            for (const auto& component : object->m_components)
            {
                remapObject(*component);
            }
        }
        for (const auto& subsystem : m_subsystems)
        {
            remapObject(*subsystem);
        }
    }

    void Level::RemoveEmptyEngineObjects()
    {
        m_engineObjects.erase(std::remove(m_engineObjects.begin(), m_engineObjects.end(), nullptr), m_engineObjects.end());
    }

    LevelCollection* Level::GetRuntimeCollection()
    {
        LevelCollection* collection = FindCollectionByName(RuntimeCollectionName);
        return collection ? collection : AddCollection(RuntimeCollectionName);
    }

    LevelObject* Level::Spawn(const Assets::AssetReference<LevelObjectVariantAsset>& variant, const Math::Vector3& position, const Math::Quaternion& rotation, LevelObject* parent)
    {
        LevelCollection* collection = parent ? parent->GetCollection() : (m_isPlaying ? GetRuntimeCollection() : GetDefaultCollection());
        LevelObject* object = LevelSerializer::Instantiate(*this, variant.guid, collection, parent, {}, false);
        if (!object)
        {
            return nullptr;
        }
        object->GetTransform().SetPositionAndRotation(position, rotation);
        InitializeHierarchy(*object);
        return object;
    }

    void Level::InitializeHierarchy(LevelObject& root)
    {
        InitializeObject(root);
        std::vector<LevelObject*> children = root.GetChildren();
        for (LevelObject* child : children)
        {
            InitializeHierarchy(*child);
        }
    }
}
