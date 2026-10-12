#pragma once

#include "Math/Color.h"
#include "Math/Matrix4.h"

#include <cstdint>

namespace BrightBox3D::Rendering
{
    enum class AntiAliasing
    {
        None,
        FXAA,
        SMAA,
        TAA
    };

    struct PostProcessSettings
    {
        float exposure = 1.0f;
        bool autoExposure = false;
        float autoExposureMin = 0.25f;
        float autoExposureMax = 4.0f;
        float autoExposureSpeed = 2.0f;
        float bloomIntensity = 0.04f;
        float bloomThreshold = 1.0f;
        bool ssao = true;
        float ssaoRadius = 0.5f;
        float ssaoIntensity = 1.0f;
        bool colorGrading = false;
        float saturation = 1.0f;
        float contrast = 1.0f;
        Math::Color tint = Math::Color::White();
        Math::Color lift = Math::Color::Black();
        Math::Color gamma = Math::Color::White();
        Math::Color gain = Math::Color::White();
        float vignetteIntensity = 0.0f;
        float vignetteRadius = 0.8f;
        float vignetteSoftness = 0.45f;
        Math::Color vignetteColor = Math::Color::Black();
        AntiAliasing antiAliasing = AntiAliasing::TAA;
    };

    struct EnvironmentSettings
    {
        Math::Color skyTop{ 0.32f, 0.52f, 0.85f, 1.0f };
        Math::Color skyHorizon{ 0.75f, 0.82f, 0.9f, 1.0f };
        Math::Color skyGround{ 0.28f, 0.26f, 0.24f, 1.0f };
        float skyIntensity = 1.0f;
        float ambientDiffuse = 0.6f;
        float ambientSpecular = 0.5f;
    };

    struct ShadowSettings
    {
        bool enabled = true;
        uint32_t resolution = 2048;
        float distance = 80.0f;
        float splitLambda = 0.75f;
        float bias = 0.0005f;
        float normalBias = 1.5f;
    };

    enum class RenderDebugMode
    {
        Lit,
        Albedo,
        Normals,
        MetallicRoughness,
        Shadows,
        AmbientOcclusion
    };

    struct RenderView
    {
        uint64_t id = 0;
        Math::Matrix4 view;
        Math::Matrix4 projection;
        Math::Vector3 position;
        float verticalFov = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        uint32_t width = 1;
        uint32_t height = 1;
        PostProcessSettings post;
        EnvironmentSettings environment;
        ShadowSettings shadows;
        RenderDebugMode debugMode = RenderDebugMode::Lit;
        bool showGrid = false;
        bool isEditorView = false;
    };
}
