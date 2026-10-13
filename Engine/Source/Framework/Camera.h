#pragma once

#include "Assets/AssetDatabase.h"
#include "Framework/Categories.h"
#include "Level/Categories.h"
#include "Level/LevelObject.h"
#include "Rendering/Categories.h"
#include "Rendering/RenderView.h"
#include "UI/UIAssets.h"

namespace BrightBox3D::Framework
{
    BrightEnum()
    enum class CameraAntiAliasing
    {
        None,
        FXAA,
        SMAA,
        TAA
    };

    BrightClass(.category = Categories::Gameplay, .icon = Icon::Video)
    class Camera : public Level::LevelObject
    {
        BrightBody()
    public:
        Rendering::RenderView BuildRenderView(uint32_t width, uint32_t height, uint64_t viewId) const;
        static Camera* FindActive(const Level::Level& level);

        float GetFieldOfView() const { return m_fieldOfView; }
        void SetFieldOfView(float degrees) { m_fieldOfView = degrees; }
        int32_t GetPriority() const { return m_priority; }
        void SetPriority(int32_t priority) { m_priority = priority; }
        const Assets::AssetReference<UI::ScreenAsset>& GetActiveScreen() const { return m_activeScreen; }
        void SetActiveScreen(const Assets::AssetReference<UI::ScreenAsset>& screen) { m_activeScreen = screen; }

    private:
        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Projection)
        float m_fieldOfView = 60.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Projection)
        float m_nearPlane = 0.1f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Projection)
        float m_farPlane = 1000.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Camera)
        int32_t m_priority = 0;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::UI)
        Assets::AssetReference<UI::ScreenAsset> m_activeScreen;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Exposure)
        float m_exposure = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Exposure)
        bool m_autoExposure = false;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Exposure)
        float m_autoExposureMin = 0.25f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Exposure)
        float m_autoExposureMax = 4.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Bloom)
        float m_bloomIntensity = 0.04f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Bloom)
        float m_bloomThreshold = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::AmbientOcclusion)
        bool m_ssao = true;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::AmbientOcclusion)
        float m_ssaoRadius = 0.5f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::AmbientOcclusion)
        float m_ssaoIntensity = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::ColorGrading)
        bool m_colorGrading = false;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::ColorGrading)
        float m_saturation = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::ColorGrading)
        float m_contrast = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::ColorGrading)
        Math::Color m_tint = Math::Color::White();

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::ColorGrading)
        Math::Color m_lift = Math::Color::Black();

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::ColorGrading)
        Math::Color m_gamma = Math::Color::White();

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::ColorGrading)
        Math::Color m_gain = Math::Color::White();

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Vignette)
        float m_vignetteIntensity = 0.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Vignette)
        float m_vignetteRadius = 0.8f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Vignette)
        float m_vignetteSoftness = 0.45f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::AntiAliasing)
        CameraAntiAliasing m_antiAliasing = CameraAntiAliasing::TAA;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Sky)
        Math::Color m_skyTop{ 0.32f, 0.52f, 0.85f, 1.0f };

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Sky)
        Math::Color m_skyHorizon{ 0.75f, 0.82f, 0.9f, 1.0f };

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Sky)
        Math::Color m_skyGround{ 0.28f, 0.26f, 0.24f, 1.0f };

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Sky)
        float m_skyIntensity = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Sky)
        float m_ambientIntensity = 0.6f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Shadows)
        float m_shadowDistance = 80.0f;
    };
}
