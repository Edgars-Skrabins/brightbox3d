#pragma once

#include "RHI/Types.h"

namespace BrightBox3D::RHI
{
    class Resource
    {
    public:
        virtual ~Resource() = default;
    };

    class Buffer : public Resource
    {
    public:
        explicit Buffer(const BufferDesc& desc) : m_desc(desc) {}
        const BufferDesc& GetDesc() const { return m_desc; }
        size_t GetSize() const { return m_desc.size; }

    protected:
        BufferDesc m_desc;
    };

    class Texture : public Resource
    {
    public:
        explicit Texture(const TextureDesc& desc) : m_desc(desc) {}
        const TextureDesc& GetDesc() const { return m_desc; }
        uint32_t GetWidth() const { return m_desc.width; }
        uint32_t GetHeight() const { return m_desc.height; }
        Format GetFormat() const { return m_desc.format; }

    protected:
        TextureDesc m_desc;
    };

    class Sampler : public Resource
    {
    public:
        explicit Sampler(const SamplerDesc& desc) : m_desc(desc) {}
        const SamplerDesc& GetDesc() const { return m_desc; }

    protected:
        SamplerDesc m_desc;
    };

    class Shader : public Resource
    {
    public:
        explicit Shader(ShaderStage stage) : m_stage(stage) {}
        ShaderStage GetStage() const { return m_stage; }

    protected:
        ShaderStage m_stage;
    };

    class Pipeline : public Resource
    {
    public:
        explicit Pipeline(const PipelineDesc& desc) : m_desc(desc) {}
        const PipelineDesc& GetDesc() const { return m_desc; }

    protected:
        PipelineDesc m_desc;
    };

    class Swapchain : public Resource
    {
    public:
        virtual Texture* AcquireNextTexture() = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual Format GetFormat() const = 0;
        virtual void SetVSync(bool enabled) = 0;
    };
}
