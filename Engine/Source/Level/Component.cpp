#include "Level/Component.h"

#include "Level/Level.h"
#include "Level/LevelObjectInstance.h"

namespace BrightBox3D::Level
{
    Transform& Component::GetTransform() const
    {
        return m_owner->GetTransform();
    }

    Level* Component::GetLevel() const
    {
        return m_owner ? m_owner->GetLevel() : nullptr;
    }

    void Component::SetEnabled(bool enabled)
    {
        if (m_isEnabled == enabled)
        {
            return;
        }
        m_isEnabled = enabled;
        if (Level* level = GetLevel())
        {
            level->RefreshEnabledState(*m_owner);
        }
    }

    bool Component::IsActiveAndEnabled() const
    {
        return m_isEnabled && m_owner && m_owner->IsActiveInHierarchy();
    }
}
