#include "Level/Level.h"

#include "Core/Log.h"

#include <algorithm>
#include <mutex>

namespace BrightBox3D::Level
{
    namespace
    {
        constexpr int MaxFixedStepsPerFrame = 8;

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
        std::vector<LevelObjectInstance*> roots = collection->GetRoots();
        for (LevelObjectInstance* root : roots)
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

    LevelObjectInstance* Level::CreateObject(const Reflection::TypeInfo& type, LevelCollection* collection, LevelObjectInstance* parent, const Core::Guid& id, bool initialize)
    {
        if (!type.IsA(LevelObjectInstance::StaticType()))
        {
            Core::Log::Error("'" + type.GetName() + "' is not a LevelObjectInstance type");
            return nullptr;
        }
        std::unique_ptr<Reflection::Object> created(type.CreateInstance());
        if (!created)
        {
            Core::Log::Error("'" + type.GetName() + "' is abstract and cannot be created");
            return nullptr;
        }
        std::unique_ptr<LevelObjectInstance> object(static_cast<LevelObjectInstance*>(created.release()));
        if (id.IsValid())
        {
            object->SetId(id);
        }
        object->m_name = type.GetName() == "LevelObjectInstance" ? "LevelObject" : type.GetName();
        return AdoptObject(std::move(object), collection, parent, initialize);
    }

    LevelObjectInstance* Level::AdoptObject(std::unique_ptr<LevelObjectInstance> object, LevelCollection* collection, LevelObjectInstance* parent, bool initialize)
    {
        if (m_index.count(object->GetId()))
        {
            Core::Log::Warning("Object id collision for '" + object->GetName() + "'; assigning a new id");
            object->SetId(Core::Guid::Generate());
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

        LevelObjectInstance* raw = object.get();
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

    void Level::InitializeObject(LevelObjectInstance& object)
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

    void Level::ActivateForPlay(LevelObjectInstance& object)
    {
        if (!object.IsActiveInHierarchy() || object.m_isPendingDestroy)
        {
            return;
        }
        if (!object.m_hasLoaded)
        {
            object.m_hasLoaded = true;
            object.OnGameLoad();
        }
        for (size_t i = 0; i < object.m_components.size(); ++i)
        {
            LoadComponent(*object.m_components[i]);
        }
        ApplyEnabledState(object, true);
    }

    void Level::LoadComponent(Component& component)
    {
        if (component.m_hasLoaded || component.m_isPendingDestroy)
        {
            return;
        }
        component.m_hasLoaded = true;
        component.OnGameLoad();
    }

    void Level::ApplyEnabledState(LevelObjectInstance& object, bool notify)
    {
        bool objectEnabled = object.IsActiveInHierarchy() && !object.m_isPendingDestroy;
        if (objectEnabled && !object.m_hasLoaded && m_isPlaying)
        {
            object.m_hasLoaded = true;
            object.OnGameLoad();
        }
        if (objectEnabled != object.m_isEnabledState)
        {
            object.m_isEnabledState = objectEnabled;
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
            Component& component = *object.m_components[i];
            bool componentEnabled = objectEnabled && component.m_isEnabled && !component.m_isPendingDestroy;
            if (componentEnabled && m_isPlaying)
            {
                LoadComponent(component);
            }
            if (componentEnabled != component.m_isEnabledState)
            {
                component.m_isEnabledState = componentEnabled;
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

    void Level::RefreshEnabledState(LevelObjectInstance& object)
    {
        ApplyEnabledState(object, true);
        std::vector<LevelObjectInstance*> children = object.m_children;
        for (LevelObjectInstance* child : children)
        {
            RefreshEnabledState(*child);
        }
    }

    void Level::OnComponentAttached(Component& component)
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

    void Level::RemoveComponent(Component& component)
    {
        if (component.m_isPendingDestroy)
        {
            return;
        }
        if (m_isPlaying && !m_isEnding)
        {
            component.m_isPendingDestroy = true;
            m_pendingComponentDestroy.push_back(&component);
            return;
        }
        LevelObjectInstance* owner = component.m_owner;
        if (m_isPlaying)
        {
            if (component.m_isEnabledState)
            {
                component.m_isEnabledState = false;
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

    void Level::MoveToCollection(LevelObjectInstance& object, LevelCollection* collection)
    {
        object.m_collection = collection;
        for (LevelObjectInstance* child : object.m_children)
        {
            MoveToCollection(*child, collection);
        }
    }

    void Level::SetRootCollection(LevelObjectInstance& root, LevelCollection* collection, int siblingIndex)
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

    void Level::DestroyObject(LevelObjectInstance* object)
    {
        if (!object || object->m_level != this || object->m_isPendingDestroy)
        {
            return;
        }
        if (!m_isPlaying || m_isEnding)
        {
            DestroyObjectImmediate(object);
            return;
        }
        std::function<void(LevelObjectInstance&)> mark = [&](LevelObjectInstance& target)
        {
            target.m_isPendingDestroy = true;
            for (LevelObjectInstance* child : target.m_children)
            {
                mark(*child);
            }
        };
        mark(*object);
        m_pendingDestroy.push_back(object);
    }

    void Level::DestroyRecursive(LevelObjectInstance& object, bool callLifecycle, std::vector<LevelObjectInstance*>& order)
    {
        object.m_isPendingDestroy = true;
        std::vector<LevelObjectInstance*> children = object.m_children;
        for (LevelObjectInstance* child : children)
        {
            DestroyRecursive(*child, callLifecycle, order);
        }
        if (callLifecycle)
        {
            for (const auto& component : object.m_components)
            {
                if (component->m_isEnabledState)
                {
                    component->m_isEnabledState = false;
                    component->OnDisable();
                }
            }
            if (object.m_isEnabledState)
            {
                object.m_isEnabledState = false;
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

    void Level::FinalizeDestroy(const std::vector<LevelObjectInstance*>& order)
    {
        for (LevelObjectInstance* object : order)
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
        std::unordered_map<LevelObjectInstance*, bool> doomed;
        for (LevelObjectInstance* object : order)
        {
            doomed[object] = true;
        }
        m_pendingDestroy.erase(std::remove_if(m_pendingDestroy.begin(), m_pendingDestroy.end(), [&](LevelObjectInstance* object) { return doomed.count(object) > 0; }), m_pendingDestroy.end());
        m_pendingComponentDestroy.erase(std::remove_if(m_pendingComponentDestroy.begin(), m_pendingComponentDestroy.end(), [&](Component* component) { return doomed.count(component->GetOwner()) > 0; }), m_pendingComponentDestroy.end());
        m_objects.erase(std::remove_if(m_objects.begin(), m_objects.end(), [&](const auto& entry) { return doomed.count(entry.get()) > 0; }), m_objects.end());
        MarkHierarchyChanged();
    }

    void Level::DestroyObjectImmediate(LevelObjectInstance* object)
    {
        if (!object || object->m_level != this)
        {
            return;
        }
        std::vector<LevelObjectInstance*> order;
        DestroyRecursive(*object, m_isPlaying, order);
        FinalizeDestroy(order);
    }

    void Level::FlushDestroyed()
    {
        if (!m_pendingComponentDestroy.empty())
        {
            std::vector<Component*> components;
            components.swap(m_pendingComponentDestroy);
            for (Component* component : components)
            {
                LevelObjectInstance* owner = component->m_owner;
                if (!owner || owner->m_isPendingDestroy)
                {
                    continue;
                }
                if (component->m_isEnabledState)
                {
                    component->m_isEnabledState = false;
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
            std::vector<LevelObjectInstance*> roots = m_pendingDestroy;
            std::vector<LevelObjectInstance*> order;
            for (LevelObjectInstance* root : roots)
            {
                bool nested = false;
                for (LevelObjectInstance* other : roots)
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

    LevelObjectInstance* Level::FindObject(const Core::Guid& id) const
    {
        auto it = m_index.find(id);
        return it == m_index.end() ? nullptr : it->second;
    }

    LevelObjectInstance* Level::FindObjectByName(const std::string& name) const
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

    void Level::ForEachObject(const std::function<void(LevelObjectInstance&)>& visitor) const
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
            subsystem->OnBeginPlay();
        }
        size_t count = m_objects.size();
        for (size_t i = 0; i < count && i < m_objects.size(); ++i)
        {
            ActivateForPlay(*m_objects[i]);
        }
    }

    void Level::RunPendingStarts()
    {
        size_t count = m_objects.size();
        for (size_t i = 0; i < count && i < m_objects.size(); ++i)
        {
            LevelObjectInstance& object = *m_objects[i];
            if (object.m_isPendingDestroy || !object.m_isEnabledState)
            {
                continue;
            }
            if (!object.m_hasStarted)
            {
                object.m_hasStarted = true;
                object.OnGameStart();
            }
            for (size_t c = 0; c < object.m_components.size(); ++c)
            {
                Component& component = *object.m_components[c];
                if (component.m_isEnabledState && !component.m_hasStarted && !component.m_isPendingDestroy)
                {
                    component.m_hasStarted = true;
                    component.OnGameStart();
                }
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
        deltaTime = std::clamp(deltaTime, 0.0f, 0.25f);
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
            size_t count = m_objects.size();
            for (size_t i = 0; i < count && i < m_objects.size(); ++i)
            {
                LevelObjectInstance& object = *m_objects[i];
                if (!object.m_isEnabledState || object.m_isPendingDestroy || !object.m_hasStarted)
                {
                    continue;
                }
                object.OnGameUpdateFixed(m_fixedDeltaTime);
                for (size_t c = 0; c < object.m_components.size(); ++c)
                {
                    Component& component = *object.m_components[c];
                    if (component.m_isEnabledState && component.m_hasStarted && !component.m_isPendingDestroy)
                    {
                        component.OnGameUpdateFixed(m_fixedDeltaTime);
                    }
                }
            }
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnFixedUpdate(m_fixedDeltaTime);
            }
            m_fixedAccumulator -= m_fixedDeltaTime;
            ++steps;
        }
        if (steps == MaxFixedStepsPerFrame)
        {
            m_fixedAccumulator = std::min(m_fixedAccumulator, m_fixedDeltaTime);
        }

        size_t count = m_objects.size();
        for (size_t i = 0; i < count && i < m_objects.size(); ++i)
        {
            LevelObjectInstance& object = *m_objects[i];
            if (!object.m_isEnabledState || object.m_isPendingDestroy || !object.m_hasStarted)
            {
                continue;
            }
            object.OnGameUpdate(m_deltaTime);
            for (size_t c = 0; c < object.m_components.size(); ++c)
            {
                Component& component = *object.m_components[c];
                if (component.m_isEnabledState && component.m_hasStarted && !component.m_isPendingDestroy)
                {
                    component.OnGameUpdate(m_deltaTime);
                }
            }
        }
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnUpdate(m_deltaTime);
        }

        count = m_objects.size();
        for (size_t i = 0; i < count && i < m_objects.size(); ++i)
        {
            LevelObjectInstance& object = *m_objects[i];
            if (!object.m_isEnabledState || object.m_isPendingDestroy || !object.m_hasStarted)
            {
                continue;
            }
            object.OnGameUpdateLate(m_deltaTime);
            for (size_t c = 0; c < object.m_components.size(); ++c)
            {
                Component& component = *object.m_components[c];
                if (component.m_isEnabledState && component.m_hasStarted && !component.m_isPendingDestroy)
                {
                    component.OnGameUpdateLate(m_deltaTime);
                }
            }
        }
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnLateUpdate(m_deltaTime);
        }

        FlushDestroyed();
    }

    void Level::EndPlay()
    {
        if (!m_isPlaying)
        {
            return;
        }
        FlushDestroyed();
        m_isEnding = true;
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            LevelObjectInstance& object = *m_objects[i];
            for (const auto& component : object.m_components)
            {
                if (component->m_isEnabledState)
                {
                    component->m_isEnabledState = false;
                    component->OnDisable();
                }
            }
            if (object.m_isEnabledState)
            {
                object.m_isEnabledState = false;
                object.OnDisable();
            }
        }
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            LevelObjectInstance& object = *m_objects[i];
            if (!object.m_hasLoaded)
            {
                continue;
            }
            object.OnGameEnd();
            for (const auto& component : object.m_components)
            {
                if (component->m_hasLoaded)
                {
                    component->OnGameEnd();
                }
            }
        }
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            LevelObjectInstance& object = *m_objects[i];
            for (const auto& component : object.m_components)
            {
                if (component->m_hasLoaded)
                {
                    component->OnDestroy();
                }
            }
            if (object.m_hasLoaded)
            {
                object.OnDestroy();
            }
        }
        for (auto& subsystem : m_subsystems)
        {
            subsystem->OnEndPlay();
        }
        m_isEnding = false;
        m_isPlaying = false;
    }

    std::unique_ptr<LevelObjectInstance> Level::ReleaseObjectForReplacement(LevelObjectInstance& object)
    {
        for (const auto& component : object.m_components)
        {
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnComponentRemoved(*component);
            }
        }
        if (object.m_parent)
        {
            auto& siblings = object.m_parent->m_children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), &object), siblings.end());
        }
        else if (object.m_collection)
        {
            object.m_collection->RemoveRoot(&object);
        }
        m_index.erase(object.GetId());
        for (auto it = m_objects.begin(); it != m_objects.end(); ++it)
        {
            if (it->get() == &object)
            {
                std::unique_ptr<LevelObjectInstance> released = std::move(*it);
                m_objects.erase(it);
                return released;
            }
        }
        return nullptr;
    }

    void Level::InstallReplacement(std::unique_ptr<LevelObjectInstance> replacement, LevelObjectInstance* parent, LevelCollection* collection, int siblingIndex)
    {
        LevelObjectInstance* raw = replacement.get();
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
        for (LevelObjectInstance* child : raw->m_children)
        {
            child->m_parent = raw;
        }
        for (const auto& component : raw->m_components)
        {
            component->m_owner = raw;
            for (auto& subsystem : m_subsystems)
            {
                subsystem->OnComponentAdded(*component);
            }
        }
        raw->m_transform.MarkDirty();
        m_index[raw->GetId()] = raw;
        m_objects.push_back(std::move(replacement));
        MarkHierarchyChanged();
    }
}
