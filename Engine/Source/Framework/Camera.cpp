#include "Framework/Camera.h"

#include "Level/Level.h"

namespace BrightBox3D::Framework
{
    Rendering::RenderView Camera::BuildRenderView(uint32_t width, uint32_t height, uint64_t viewId) const
    {
        Rendering::RenderView view;
        view.id = viewId;
        view.width = std::max<uint32_t>(width, 1);
        view.height = std::max<uint32_t>(height, 1);
        const Level::Transform& transform = GetTransform();
        view.position = transform.GetPosition();
        Math::Quaternion rotation = transform.GetRotation();
        view.view = Math::Matrix4::LookAtLH(view.position, view.position + rotation * Math::Vector3::Forward(), rotation * Math::Vector3::Up());
        view.verticalFov = m_fieldOfView;
        view.nearPlane = m_nearPlane;
        view.farPlane = m_farPlane;
        view.projection = Math::Matrix4::PerspectiveFovLH(m_fieldOfView * Math::DegreesToRadians, static_cast<float>(view.width) / static_cast<float>(view.height), m_nearPlane, m_farPlane);

        Rendering::PostProcessSettings& post = view.post;
        post.exposure = m_exposure;
        post.autoExposure = m_autoExposure;
        post.autoExposureMin = m_autoExposureMin;
        post.autoExposureMax = m_autoExposureMax;
        post.bloomIntensity = m_bloomIntensity;
        post.bloomThreshold = m_bloomThreshold;
        post.ssao = m_ssao;
        post.ssaoRadius = m_ssaoRadius;
        post.ssaoIntensity = m_ssaoIntensity;
        post.colorGrading = m_colorGrading;
        post.saturation = m_saturation;
        post.contrast = m_contrast;
        post.tint = m_tint;
        post.lift = m_lift;
        post.gamma = m_gamma;
        post.gain = m_gain;
        post.vignetteIntensity = m_vignetteIntensity;
        post.vignetteRadius = m_vignetteRadius;
        post.vignetteSoftness = m_vignetteSoftness;
        post.antiAliasing = m_antiAliasing;

        view.environment.skyTop = m_skyTop;
        view.environment.skyHorizon = m_skyHorizon;
        view.environment.skyGround = m_skyGround;
        view.environment.skyIntensity = m_skyIntensity;
        view.environment.ambientDiffuse = m_ambientIntensity;
        view.shadows.distance = m_shadowDistance;
        return view;
    }

    Camera* Camera::FindActive(const Level::Level& level)
    {
        Camera* best = nullptr;
        for (Camera* camera : level.FindObjectsOfType<Camera>())
        {
            if (!camera->IsActiveInHierarchy())
            {
                continue;
            }
            if (!best || camera->m_priority > best->m_priority)
            {
                best = camera;
            }
        }
        return best;
    }
}
