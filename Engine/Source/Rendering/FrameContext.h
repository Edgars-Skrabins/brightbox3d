#pragma once

#include "Math/Matrix4.h"
#include "Rendering/DrawCollector.h"
#include "Rendering/FrameLighting.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/RenderTargets.h"

#include <vector>

namespace BrightBox3D::Rendering
{
    struct RenderView;

    struct FrameContext
    {
        FrameContext(RHI::CommandList& frameCommands, const RenderView& frameView, RenderTargets& frameTargets, FrameLighting& frameLighting, const std::vector<DrawItem>& frameDrawItems, RHI::Texture* frameOutput)
            : commands(frameCommands), view(frameView), targets(frameTargets), lighting(frameLighting), drawItems(frameDrawItems), output(frameOutput)
        {
        }

        RHI::CommandList& commands;
        const RenderView& view;
        RenderTargets& targets;
        FrameLighting& lighting;
        const std::vector<DrawItem>& drawItems;
        RHI::Texture* output = nullptr;
        ViewConstants viewConstants{};
        Math::Matrix4 viewProjection;
        std::vector<const DrawItem*> opaque;
        std::vector<const DrawItem*> transparent;
        RHI::Texture* ambientOcclusion = nullptr;
    };
}
