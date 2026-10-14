#pragma once

#include "RHI/Resources.h"

#include <d3d11_1.h>
#include <wrl/client.h>

#include <cstdint>
#include <unordered_map>

namespace BrightBox3D::RHI
{
    using Microsoft::WRL::ComPtr;

    class DX11Buffer : public Buffer
    {
    public:
        DX11Buffer(const BufferDesc& desc, ComPtr<ID3D11Buffer> buffer) : Buffer(desc), m_buffer(std::move(buffer)) {}
        ID3D11Buffer* Get() const { return m_buffer.Get(); }

    private:
        ComPtr<ID3D11Buffer> m_buffer;
    };

    class DX11Texture : public Texture
    {
    public:
        explicit DX11Texture(const TextureDesc& desc) : Texture(desc) {}

        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> shaderView;
        std::unordered_map<uint32_t, ComPtr<ID3D11RenderTargetView>> renderViews;
        std::unordered_map<uint32_t, ComPtr<ID3D11DepthStencilView>> depthViews;

        void SetSize(uint32_t width, uint32_t height)
        {
            m_desc.width = width;
            m_desc.height = height;
        }

        ID3D11RenderTargetView* GetRenderView(ID3D11Device* device, uint32_t mip, uint32_t layer);
        ID3D11DepthStencilView* GetDepthView(ID3D11Device* device, uint32_t layer);
    };

    class DX11Sampler : public Sampler
    {
    public:
        DX11Sampler(const SamplerDesc& desc, ComPtr<ID3D11SamplerState> state) : Sampler(desc), m_state(std::move(state)) {}
        ID3D11SamplerState* Get() const { return m_state.Get(); }

    private:
        ComPtr<ID3D11SamplerState> m_state;
    };

    class DX11Shader : public Shader
    {
    public:
        DX11Shader(ShaderStage stage, ComPtr<ID3DBlob> bytecode) : Shader(stage), bytecode(std::move(bytecode)) {}

        ComPtr<ID3DBlob> bytecode;
        ComPtr<ID3D11VertexShader> vertex;
        ComPtr<ID3D11PixelShader> pixel;
    };

    class DX11Pipeline : public Pipeline
    {
    public:
        explicit DX11Pipeline(const PipelineDesc& desc) : Pipeline(desc) {}

        ComPtr<ID3D11InputLayout> inputLayout;
        ComPtr<ID3D11RasterizerState> rasterizer;
        ComPtr<ID3D11DepthStencilState> depthStencil;
        ComPtr<ID3D11BlendState> blend;
    };
}
