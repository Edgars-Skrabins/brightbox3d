#include "Runtime/RenderContext.h"

#include "Core/EngineLayout.h"
#include "Core/FileSystem.h"
#include "UI/ScreenRenderer.h"

namespace BrightBox3D::Runtime
{
    namespace
    {
        constexpr RHI::PresentMode WindowPresentMode = RHI::PresentMode::VSync;
    }

    std::unique_ptr<RenderContext> RenderContext::Create(Platform::Window* window, const std::filesystem::path& shaderDirectory)
    {
        std::unique_ptr<RHI::Device> device = RHI::Device::Create();
        if (!device)
        {
            return nullptr;
        }
        std::unique_ptr<RenderContext> context(new RenderContext());
        context->m_device = std::move(device);
        if (window)
        {
            context->m_swapchain = context->m_device->CreateSwapchain(*window, WindowPresentMode);
        }
        context->m_shaders = std::make_unique<Rendering::ShaderLibrary>(*context->m_device, shaderDirectory);
        context->m_resources = std::make_unique<Rendering::GpuResourceCache>(*context->m_device);
        context->m_renderer = std::make_unique<Rendering::Renderer>(*context->m_device, *context->m_shaders, *context->m_resources);
        UI::ScreenRenderer::Get().Initialize(*context->m_device, *context->m_shaders, *context->m_resources);
        return context;
    }

    std::filesystem::path RenderContext::DefaultShaderDirectory()
    {
        return Core::FileSystem::EngineContentDirectory() / Core::EngineLayout::ShadersFolder;
    }

    RenderContext::~RenderContext()
    {
        m_device->WaitIdle();
        UI::ScreenRenderer::Get().Shutdown();
        m_renderer.reset();
        m_resources.reset();
        m_shaders.reset();
        m_swapchain.reset();
        m_device.reset();
    }
}
