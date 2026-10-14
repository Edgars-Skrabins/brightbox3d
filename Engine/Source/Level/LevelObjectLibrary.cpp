#include "Level/LevelSerializer.h"

#include "Level/LevelObjectLinking.h"
#include "Level/LevelReader.h"

namespace BrightBox3D::Level
{
    namespace
    {
        constexpr const char* PrototypeLevelName = "Prototype";
    }

    LevelObjectLibrary& LevelObjectLibrary::Get()
    {
        static LevelObjectLibrary* library = new LevelObjectLibrary();
        return *library;
    }

    LevelObjectLibrary::LevelObjectLibrary() = default;

    LevelObject* LevelObjectLibrary::GetPrototype(const Core::Guid& asset)
    {
        auto it = m_entries.find(asset);
        if (it != m_entries.end())
        {
            return it->second.root;
        }
        Entry entry;
        entry.level = std::make_unique<Level>(PrototypeLevelName);
        entry.root = LevelReader::InstantiateAsset(*entry.level, asset, nullptr, nullptr, Core::Guid::Generate(), false, true);
        if (!entry.root)
        {
            return nullptr;
        }
        entry.bySource = LevelObjectLinking::IndexBySource(*entry.root);
        LevelObject* root = entry.root;
        m_entries[asset] = std::move(entry);
        return root;
    }

    LevelObject* LevelObjectLibrary::FindPrototypeNode(const Core::Guid& asset, const Core::Guid& sourceId)
    {
        if (!GetPrototype(asset))
        {
            return nullptr;
        }
        auto& bySource = m_entries[asset].bySource;
        auto it = bySource.find(sourceId);
        return it == bySource.end() ? nullptr : it->second;
    }

    void LevelObjectLibrary::Invalidate(const Core::Guid& asset)
    {
        m_entries.erase(asset);
    }

    void LevelObjectLibrary::Clear()
    {
        m_entries.clear();
    }
}
