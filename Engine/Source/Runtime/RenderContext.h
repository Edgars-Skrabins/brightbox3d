#pragma once

#include "Platform/Window.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/Renderer.h"
#include "Rendering/ShaderLibrary.h"
#include "RHI/Device.h"

#include <filesystem>
#include <memory>

namespace BrightBox3D::Runtime
{
    class RenderContext
    {
    public:
        static std::unique_ptr<RenderContext> Create(Platform::Window* window, const std::filesystem::path& shaderDirectory);
        static std::filesystem::path DefaultShaderDirectory();

        ~RenderContext();
        RenderContext(const RenderContext&) = delete;
        RenderContext& operator=(const RenderContext&) = delete;

        RHI::Device& GetDevice() { return *m_device; }
        RHI::Swapchain* GetSwapchain() { return m_swapchain.get(); }
        Rendering::ShaderLibrary& GetShaders() { return *m_shaders; }
        Rendering::GpuResourceCache& GetResources() { return *m_resources; }
        Rendering::Renderer& GetRenderer() { return *m_renderer; }

    private:
        RenderContext() = default;

        std::unique_ptr<RHI::Device> m_device;
        std::unique_ptr<RHI::Swapchain> m_swapchain;
        std::unique_ptr<Rendering::ShaderLibrary> m_shaders;
        std::unique_ptr<Rendering::GpuResourceCache> m_resources;
        std::unique_ptr<Rendering::Renderer> m_renderer;
    };
}
