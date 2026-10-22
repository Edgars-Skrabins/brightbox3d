#pragma once

#include "Level/LevelObjectComponent.h"
#include "Math/Color.h"
#include "Rendering/Categories.h"

#include <cstdint>

namespace BrightBox3D::Rendering
{
    BrightEnum()
    enum class LightType
    {
        Directional,
        Point,
        Spot
    };

    BrightEnum()
    enum class ShadowResolution
    {
        Low,
        Medium,
        High
    };

    uint32_t ShadowResolutionSize(ShadowResolution resolution);

    BrightClass(.category = Categories::Rendering, .icon = Icon::Lightbulb)
    class Light : public Level::LevelObjectComponent
    {
        BrightBody()
    public:
        bool IsPropertyVisible(const Reflection::PropertyInfo& property) const override;

        LightType GetLightType() const { return m_type; }
        void SetLightType(LightType type) { m_type = type; }
        const Math::Color& GetColor() const { return m_color; }
        void SetColor(const Math::Color& color) { m_color = color; }
        float GetIntensity() const { return m_intensity; }
        void SetIntensity(float intensity) { m_intensity = intensity; }
        float GetRange() const { return m_range; }
        void SetRange(float range) { m_range = range; }
        float GetInnerAngle() const { return m_innerAngle; }
        float GetOuterAngle() const { return m_outerAngle; }
        void SetSpotAngles(float innerDegrees, float outerDegrees)
        {
            m_innerAngle = innerDegrees;
            m_outerAngle = outerDegrees;
        }
        bool GetCastShadows() const { return m_castShadows; }
        void SetCastShadows(bool castShadows) { m_castShadows = castShadows; }
        ShadowResolution GetShadowResolution() const { return m_shadowResolution; }
        void SetShadowResolution(ShadowResolution resolution) { m_shadowResolution = resolution; }
        float GetShadowBias() const { return m_shadowBias; }
        void SetShadowBias(float bias) { m_shadowBias = bias; }
        float GetShadowNormalBias() const { return m_shadowNormalBias; }
        void SetShadowNormalBias(float normalBias) { m_shadowNormalBias = normalBias; }

    private:
        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Light)
        LightType m_type = LightType::Point;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Light)
        Math::Color m_color = Math::Color::White();

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Light)
        float m_intensity = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Light)
        float m_range = 10.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Spot)
        float m_innerAngle = 25.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Spot)
        float m_outerAngle = 35.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Shadows)
        bool m_castShadows = true;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Shadows)
        ShadowResolution m_shadowResolution = ShadowResolution::Medium;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Shadows)
        float m_shadowBias = 0.0005f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Shadows)
        float m_shadowNormalBias = 1.5f;
    };
}
