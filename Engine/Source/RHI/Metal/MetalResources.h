#pragma once

#include "RHI/Resources.h"

#import <Metal/Metal.h>

#include <cstdint>

namespace BrightBox3D::RHI
{
    constexpr uint32_t FramesInFlight = 3;
    constexpr uint32_t VertexBufferBase = 16;

    class MetalBuffer : public Buffer
    {
    public:
        MetalBuffer(id<MTLDevice> device, const BufferDesc& desc, const void* initialData);

        id<MTLBuffer> Get(uint32_t frame) const { return m_desc.dynamic ? m_buffers[frame % FramesInFlight] : m_buffers[0]; }

    private:
        id<MTLBuffer> m_buffers[FramesInFlight];
    };

    class MetalTexture : public Texture
    {
    public:
        MetalTexture(id<MTLDevice> device, const TextureDesc& desc);
        MetalTexture(id<MTLTexture> texture, const TextureDesc& desc) : Texture(desc), m_texture(texture) {}

        id<MTLTexture> Get() const { return m_texture; }
        void Set(id<MTLTexture> texture) { m_texture = texture; }
        void SetSize(uint32_t width, uint32_t height)
        {
            m_desc.width = width;
            m_desc.height = height;
        }

    private:
        id<MTLTexture> m_texture;
    };

    class MetalSampler : public Sampler
    {
    public:
        MetalSampler(id<MTLDevice> device, const SamplerDesc& desc);

        id<MTLSamplerState> Get() const { return m_sampler; }

    private:
        id<MTLSamplerState> m_sampler;
    };

    class MetalShader : public Shader
    {
    public:
        MetalShader(ShaderStage stage, id<MTLFunction> function) : Shader(stage), m_function(function) {}

        id<MTLFunction> Get() const { return m_function; }

    private:
        id<MTLFunction> m_function;
    };

    class MetalPipeline : public Pipeline
    {
    public:
        MetalPipeline(const PipelineDesc& desc, id<MTLRenderPipelineState> state, id<MTLDepthStencilState> depthState) : Pipeline(desc), m_state(state), m_depthState(depthState) {}

        id<MTLRenderPipelineState> GetState() const { return m_state; }
        id<MTLDepthStencilState> GetDepthState() const { return m_depthState; }

    private:
        id<MTLRenderPipelineState> m_state;
        id<MTLDepthStencilState> m_depthState;
    };
}
