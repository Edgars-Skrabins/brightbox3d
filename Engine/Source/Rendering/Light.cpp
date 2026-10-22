#include "Rendering/Light.h"

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint32_t LowShadowResolution = 512;
        constexpr uint32_t MediumShadowResolution = 1024;
        constexpr uint32_t HighShadowResolution = 2048;
    }

    uint32_t ShadowResolutionSize(ShadowResolution resolution)
    {
        switch (resolution)
        {
            case ShadowResolution::Low:
                return LowShadowResolution;
            case ShadowResolution::High:
                return HighShadowResolution;
            case ShadowResolution::Medium:
                break;
        }
        return MediumShadowResolution;
    }

    bool Light::IsPropertyVisible(const Reflection::PropertyInfo& property) const
    {
        const void* value = property.GetValuePointer(this);
        if (value == &m_innerAngle || value == &m_outerAngle)
        {
            return m_type == LightType::Spot;
        }
        if (value == &m_range)
        {
            return m_type != LightType::Directional;
        }
        if (value == &m_shadowResolution || value == &m_shadowBias || value == &m_shadowNormalBias)
        {
            return m_type != LightType::Directional && m_castShadows;
        }
        return true;
    }
}
