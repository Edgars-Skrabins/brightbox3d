#pragma once

#include "Core/Guid.h"

#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class Level;
    class LevelObjectInstance;

    class LevelCollection
    {
    public:
        LevelCollection(Level& level, std::string name, const Core::Guid& asset);

        Level& GetLevel() const { return m_level; }
        const std::string& GetName() const { return m_name; }
        void SetName(const std::string& name) { m_name = name; }
        const Core::Guid& GetAsset() const { return m_asset; }
        void SetAsset(const Core::Guid& asset) { m_asset = asset; }
        const std::vector<LevelObjectInstance*>& GetRoots() const { return m_roots; }
        bool IsDirty() const { return m_isDirty; }
        void SetDirty(bool dirty) { m_isDirty = dirty; }

        void AddRoot(LevelObjectInstance* object, int index = -1);
        void RemoveRoot(LevelObjectInstance* object);

    private:
        Level& m_level;
        std::string m_name;
        Core::Guid m_asset;
        std::vector<LevelObjectInstance*> m_roots;
        bool m_isDirty = false;
    };
}
