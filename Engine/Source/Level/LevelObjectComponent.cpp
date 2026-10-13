#include "Level/LevelObjectComponent.h"

#include "Level/Level.h"
#include "Level/LevelObject.h"

namespace BrightBox3D::Level
{
    Transform& LevelObjectComponent::GetTransform() const
    {
        return m_owner->GetTransform();
    }

    Level* LevelObjectComponent::GetLevel() const
    {
        return m_owner ? m_owner->GetLevel() : nullptr;
    }

    void LevelObjectComponent::SetEnabled(bool enabled)
    {
        if (m_state.isEnabled == enabled)
        {
            return;
        }
        m_state.isEnabled = enabled;
        if (Level* level = GetLevel())
        {
            level->RefreshEnabledState(*m_owner);
        }
    }

    bool LevelObjectComponent::IsActiveAndEnabled() const
    {
        return m_state.isEnabled && m_owner && m_owner->IsActiveInHierarchy();
    }
}
