#pragma once

#include "RHI/CommandList.h"
#include "RHI/Resources.h"

#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Platform
{
    class Window;
}

namespace BrightBox3D::RHI
{
    class Device
    {
    public:
        virtual ~Device() = default;

        static std::unique_ptr<Device> Create();
        static Backend GetPlatformBackend();

        virtual Backend GetBackend() const = 0;
        virtual std::string GetName() const = 0;

        virtual std::unique_ptr<Buffer> CreateBuffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
        virtual void UpdateBuffer(Buffer* buffer, const void* data, size_t size, size_t offset = 0) = 0;
        virtual std::unique_ptr<Texture> CreateTexture(const TextureDesc& desc) = 0;
        virtual void UpdateTexture(Texture* texture, uint32_t mipLevel, uint32_t layer, const void* data, uint32_t rowPitch) = 0;
        virtual std::unique_ptr<Sampler> CreateSampler(const SamplerDesc& desc) = 0;
        virtual std::unique_ptr<Shader> CreateShader(const ShaderDesc& desc, std::string& error) = 0;
        virtual std::unique_ptr<Pipeline> CreatePipeline(const PipelineDesc& desc, std::string& error) = 0;
        virtual std::unique_ptr<Swapchain> CreateSwapchain(Platform::Window& window, PresentMode presentMode) = 0;

        virtual CommandList& BeginFrame() = 0;
        virtual void EndFrame(const std::vector<Swapchain*>& present) = 0;
        virtual bool ReadTexture(Texture* texture, std::vector<uint8_t>& pixels) = 0;
        virtual void WaitIdle() = 0;
        virtual bool IsInFrame() const = 0;
    };
}
