#include "Level/LevelCollection.h"

#include <algorithm>

namespace BrightBox3D::Level
{
    LevelCollection::LevelCollection(Level& level, std::string name, const Core::Guid& asset) : m_level(level), m_name(std::move(name)), m_asset(asset)
    {
    }

    void LevelCollection::AddRoot(LevelObject* object, std::optional<size_t> position)
    {
        if (std::find(m_roots.begin(), m_roots.end(), object) != m_roots.end())
        {
            return;
        }
        if (!position || *position > m_roots.size())
        {
            m_roots.push_back(object);
        }
        else
        {
            m_roots.insert(m_roots.begin() + static_cast<std::ptrdiff_t>(*position), object);
        }
    }

    void LevelCollection::RemoveRoot(LevelObject* object)
    {
        m_roots.erase(std::remove(m_roots.begin(), m_roots.end(), object), m_roots.end());
    }
}
