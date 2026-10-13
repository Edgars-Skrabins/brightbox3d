#pragma once

#include "UI/Binding.h"

namespace BrightBox3D::RHI
{
    class CommandList;
    class Texture;
}

namespace BrightBox3D::Platform
{
    class Window;
}

namespace BrightBox3D::Framework
{
    class Camera;

    UI::BindingRoots BuildBindingRoots(const Camera& camera, Level::Level& level);
    void RenderCameraScreen(RHI::CommandList& commands, Camera& camera, Level::Level& level, RHI::Texture* target, Platform::Window* window);
}
