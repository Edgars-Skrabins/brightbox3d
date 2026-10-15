#include "Level/Level.h"

#include "Core/EngineText.h"

#include <algorithm>
#include <mutex>

namespace BrightBox3D::Level
{
    namespace
    {
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
            AddCollection(Core::Text(Core::EngineText::MainCollectionName));
        }
        return m_collections.front().get();
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

    LevelCollection* Level::GetRuntimeCollection()
    {
        LevelCollection* collection = FindCollectionByName(RuntimeCollectionName);
        return collection ? collection : AddCollection(RuntimeCollectionName);
    }
}
