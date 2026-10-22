#pragma once

#include "Math/Color.h"
#include "Math/Matrix4.h"

#include <cstdint>

namespace BrightBox3D::Rendering
{
    constexpr uint32_t MaxLights = 128;
    constexpr uint32_t CascadeCount = 4;
    constexpr uint32_t MaxLocalShadowViews = 64;
    constexpr uint32_t SSAOKernelSize = 16;

    struct ViewConstants
    {
        Math::Matrix4 view;
        Math::Matrix4 projection;
        Math::Matrix4 viewProjection;
        Math::Matrix4 inverseViewProjection;
        Math::Matrix4 unjitteredViewProjection;
        Math::Matrix4 previousViewProjection;
        float cameraPosition[4];
        float screenSize[4];
        float clipPlanes[4];
        float skyTop[4];
        float skyHorizon[4];
        float skyGround[4];
        float viewFlags[4];
    };

    struct ObjectConstants
    {
        Math::Matrix4 world;
        Math::Matrix4 previousWorld;
        Math::Matrix4 normal;
        float params[4];
    };

    struct MaterialConstants
    {
        float baseColor[4];
        float emissive[4];
        float surface[4];
        float uvTransform[4];
        float textureFlags[4];
        float materialFlags[4];
    };

    struct GpuLight
    {
        float positionRange[4];
        float directionType[4];
        float colorIntensity[4];
        float spotShadow[4];
    };

    struct LightConstants
    {
        float counts[4];
        float ambient[4];
        GpuLight lights[MaxLights];
    };

    struct ShadowConstants
    {
        Math::Matrix4 cascades[CascadeCount];
        float splits[4];
        float texelSizes[4];
        float params[4];
    };

    struct TonemapConstants
    {
        float exposure[4];
        float bloom[4];
        float grading[4];
        float tint[4];
        float vignette[4];
        float vignetteColor[4];
        float lift[4];
        float gamma[4];
        float gain[4];
    };

    struct LineConstants
    {
        float params[4];
    };

    struct PostConstants
    {
        float params[4];
        float params2[4];
        float sourceSize[4];
    };

    struct SSAOConstants
    {
        float params[4];
        float kernel[SSAOKernelSize][4];
    };

    inline void Store(float* target, float x, float y, float z, float w)
    {
        target[0] = x;
        target[1] = y;
        target[2] = z;
        target[3] = w;
    }

    inline void StoreColor(float* target, const Math::Color& color, float w)
    {
        Store(target, color.r, color.g, color.b, w);
    }
}
