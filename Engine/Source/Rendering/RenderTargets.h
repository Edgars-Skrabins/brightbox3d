#pragma once

#include "Math/Matrix4.h"
#include "RHI/Device.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::Rendering
{
    struct RenderView;

    constexpr RHI::Format AlbedoFormat = RHI::Format::RGBA8Srgb;
    constexpr RHI::Format NormalFormat = RHI::Format::RGBA16Float;
    constexpr RHI::Format MaterialFormat = RHI::Format::RGBA8Unorm;
    constexpr RHI::Format EmissionFormat = RHI::Format::RGBA16Float;
    constexpr RHI::Format VelocityFormat = RHI::Format::RG16Float;
    constexpr RHI::Format DepthCopyFormat = RHI::Format::R32Float;
    constexpr RHI::Format SceneColorFormat = RHI::Format::RGBA16Float;
    constexpr RHI::Format DepthFormat = RHI::Format::Depth32Float;
    constexpr RHI::Format AmbientOcclusionFormat = RHI::Format::R8Unorm;
    constexpr RHI::Format BloomFormat = RHI::Format::RGBA16Float;
    constexpr RHI::Format LuminanceFormat = RHI::Format::R32Float;
    constexpr RHI::Format LdrFormat = RHI::Format::RGBA8Unorm;
    constexpr RHI::Format SMAAEdgesFormat = RHI::Format::RG8Unorm;
    constexpr RHI::Format SMAABlendFormat = RHI::Format::RGBA8Unorm;
    constexpr uint32_t LuminanceSize = 64;

    struct RenderTargets
    {
        void Ensure(RHI::Device& device, const RenderView& view);

        uint32_t width = 0;
        uint32_t height = 0;
        std::unique_ptr<RHI::Texture> albedo;
        std::unique_ptr<RHI::Texture> normal;
        std::unique_ptr<RHI::Texture> material;
        std::unique_ptr<RHI::Texture> emission;
        std::unique_ptr<RHI::Texture> velocity;
        std::unique_ptr<RHI::Texture> depthCopy;
        std::unique_ptr<RHI::Texture> depth;
        std::unique_ptr<RHI::Texture> sceneColor;
        std::unique_ptr<RHI::Texture> ao;
        std::unique_ptr<RHI::Texture> aoBlur;
        std::vector<std::unique_ptr<RHI::Texture>> bloom;
        std::unique_ptr<RHI::Texture> luminance;
        std::unique_ptr<RHI::Texture> exposure[2];
        std::unique_ptr<RHI::Texture> history[2];
        std::unique_ptr<RHI::Texture> ldr;
        std::unique_ptr<RHI::Texture> smaaEdges;
        std::unique_ptr<RHI::Texture> smaaBlend;
        uint32_t historyIndex = 0;
        uint32_t exposureIndex = 0;
        uint32_t frameIndex = 0;
        bool historyValid = false;
        bool exposureValid = false;
        double lastTime = 0.0;
        Math::Matrix4 previousViewProjection;
        bool hasHistory = false;
        std::unordered_map<const void*, Math::Matrix4> previousWorld;
        std::unordered_map<const void*, Math::Matrix4> currentWorld;

    private:
        void EnsureSceneTargets(RHI::Device& device, const RenderView& view);
        void EnsurePostTargets(RHI::Device& device, const RenderView& view);
    };
}
