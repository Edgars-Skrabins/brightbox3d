#include "RHI/Device.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "RHI/DX11/DX11CommandList.h"
#include "RHI/DX11/DX11Conversions.h"
#include "RHI/DX11/DX11Resources.h"
#include "RHI/DX11/DX11Swapchain.h"
#include "RHI/ShaderCompiler.h"

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace BrightBox3D::RHI
{
    namespace
    {
        constexpr const char* VertexShaderTarget = "vs_5_0";
        constexpr const char* PixelShaderTarget = "ps_5_0";

        class DX11Device : public Device
        {
        public:
            bool Initialize()
            {
                UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
                flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
                D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
                D3D_FEATURE_LEVEL obtained{};
                HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 2, D3D11_SDK_VERSION, &m_device, &obtained, &m_context);
                if (FAILED(result))
                {
                    result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels + 1, 1, D3D11_SDK_VERSION, &m_device, &obtained, &m_context);
                }
                if (FAILED(result))
                {
                    return false;
                }
                ComPtr<IDXGIDevice> dxgiDevice;
                m_device.As(&dxgiDevice);
                ComPtr<IDXGIAdapter> adapter;
                dxgiDevice->GetAdapter(&adapter);
                adapter->GetParent(IID_PPV_ARGS(&m_factory));
                DXGI_ADAPTER_DESC adapterDesc{};
                adapter->GetDesc(&adapterDesc);
                char name[128] = {};
                WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, name, sizeof(name), nullptr, nullptr);
                m_name = std::string("DirectX 11 (") + name + ")";
                m_commandList.Initialize(m_device.Get(), m_context.Get());
                return true;
            }

            Backend GetBackend() const override { return Backend::DX11; }
            std::string GetName() const override { return m_name; }

            std::unique_ptr<Buffer> CreateBuffer(const BufferDesc& desc, const void* initialData) override
            {
                D3D11_BUFFER_DESC dx{};
                dx.ByteWidth = static_cast<UINT>(desc.usage == BufferUsage::Constant ? (desc.size + 15) / 16 * 16 : std::max<size_t>(desc.size, 4));
                dx.Usage = desc.dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
                dx.CPUAccessFlags = desc.dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
                dx.BindFlags = desc.usage == BufferUsage::Vertex ? D3D11_BIND_VERTEX_BUFFER : (desc.usage == BufferUsage::Index ? D3D11_BIND_INDEX_BUFFER : D3D11_BIND_CONSTANT_BUFFER);
                D3D11_SUBRESOURCE_DATA data{ initialData, 0, 0 };
                std::vector<uint8_t> padded;
                if (initialData && dx.ByteWidth != desc.size)
                {
                    padded.resize(dx.ByteWidth);
                    std::memcpy(padded.data(), initialData, desc.size);
                    data.pSysMem = padded.data();
                }
                ComPtr<ID3D11Buffer> buffer;
                if (FAILED(m_device->CreateBuffer(&dx, initialData ? &data : nullptr, &buffer)))
                {
                    Core::Log::Error(Core::Text(Core::EngineText::DX11BufferFailed, desc.debugName));
                    return nullptr;
                }
                return std::make_unique<DX11Buffer>(desc, buffer);
            }

            void UpdateBuffer(Buffer* buffer, const void* data, size_t size, size_t offset) override
            {
                auto* dx = static_cast<DX11Buffer*>(buffer);
                if (buffer->GetDesc().dynamic)
                {
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    if (SUCCEEDED(m_context->Map(dx->Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                    {
                        std::memcpy(static_cast<uint8_t*>(mapped.pData) + offset, data, size);
                        m_context->Unmap(dx->Get(), 0);
                    }
                    return;
                }
                D3D11_BOX box = { static_cast<UINT>(offset), 0, 0, static_cast<UINT>(offset + size), 1, 1 };
                m_context->UpdateSubresource(dx->Get(), 0, buffer->GetDesc().usage == BufferUsage::Constant ? nullptr : &box, data, 0, 0);
            }

            std::unique_ptr<Texture> CreateTexture(const TextureDesc& desc) override
            {
                auto texture = std::make_unique<DX11Texture>(desc);
                bool depth = IsDepthFormat(desc.format);
                D3D11_TEXTURE2D_DESC dx{};
                dx.Width = desc.width;
                dx.Height = desc.height;
                dx.MipLevels = std::max<uint32_t>(desc.mipLevels, 1);
                dx.ArraySize = desc.type == TextureType::TextureCube ? 6 : std::max<uint32_t>(desc.layers, 1);
                dx.Format = depth ? TypelessDepth(desc.format) : ToDXGI(desc.format);
                dx.SampleDesc.Count = 1;
                dx.Usage = D3D11_USAGE_DEFAULT;
                if (desc.usage & TextureUsageSampled)
                {
                    dx.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
                }
                if (desc.usage & TextureUsageRenderTarget)
                {
                    dx.BindFlags |= D3D11_BIND_RENDER_TARGET;
                }
                if (desc.usage & TextureUsageDepthStencil)
                {
                    dx.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
                }
                if (desc.type == TextureType::TextureCube)
                {
                    dx.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
                }
                if (dx.MipLevels > 1 && (desc.usage & TextureUsageRenderTarget) && (desc.usage & TextureUsageSampled))
                {
                    dx.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
                }
                if (FAILED(m_device->CreateTexture2D(&dx, nullptr, &texture->texture)))
                {
                    Core::Log::Error(Core::Text(Core::EngineText::DX11TextureFailed, desc.debugName));
                    return nullptr;
                }
                if (desc.usage & TextureUsageSampled)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
                    view.Format = depth ? DepthShaderView(desc.format) : ToDXGI(desc.format);
                    if (desc.type == TextureType::TextureCube)
                    {
                        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                        view.TextureCube.MipLevels = dx.MipLevels;
                    }
                    else if (desc.type == TextureType::Texture2DArray)
                    {
                        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                        view.Texture2DArray.MipLevels = dx.MipLevels;
                        view.Texture2DArray.ArraySize = dx.ArraySize;
                    }
                    else
                    {
                        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                        view.Texture2D.MipLevels = dx.MipLevels;
                    }
                    m_device->CreateShaderResourceView(texture->texture.Get(), &view, &texture->shaderView);
                }
                return texture;
            }

            void UpdateTexture(Texture* texture, uint32_t mipLevel, uint32_t layer, const void* data, uint32_t rowPitch) override
            {
                auto* dx = static_cast<DX11Texture*>(texture);
                UINT subresource = D3D11CalcSubresource(mipLevel, layer, std::max<uint32_t>(texture->GetDesc().mipLevels, 1));
                uint32_t height = std::max<uint32_t>(texture->GetHeight() >> mipLevel, 1);
                m_context->UpdateSubresource(dx->texture.Get(), subresource, nullptr, data, rowPitch, rowPitch * height);
            }

            std::unique_ptr<Sampler> CreateSampler(const SamplerDesc& desc) override
            {
                D3D11_SAMPLER_DESC dx{};
                if (desc.filter == Filter::Anisotropic)
                {
                    dx.Filter = desc.comparison ? D3D11_FILTER_COMPARISON_ANISOTROPIC : D3D11_FILTER_ANISOTROPIC;
                }
                else if (desc.filter == Filter::Point)
                {
                    dx.Filter = desc.comparison ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT : (desc.mipLinear ? D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT);
                }
                else
                {
                    dx.Filter = desc.comparison ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR : (desc.mipLinear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT);
                }
                dx.AddressU = ToD3D(desc.addressU);
                dx.AddressV = ToD3D(desc.addressV);
                dx.AddressW = ToD3D(desc.addressW);
                dx.MaxAnisotropy = std::clamp<UINT>(desc.maxAnisotropy, 1, 16);
                dx.ComparisonFunc = desc.comparison ? ToD3D(desc.compare) : D3D11_COMPARISON_NEVER;
                dx.MipLODBias = desc.mipLodBias;
                dx.MinLOD = 0.0f;
                dx.MaxLOD = D3D11_FLOAT32_MAX;
                float border = desc.borderWhite ? 1.0f : 0.0f;
                dx.BorderColor[0] = dx.BorderColor[1] = dx.BorderColor[2] = dx.BorderColor[3] = border;
                ComPtr<ID3D11SamplerState> state;
                m_device->CreateSamplerState(&dx, &state);
                return std::make_unique<DX11Sampler>(desc, state);
            }

            Core::Result<std::unique_ptr<Shader>> CreateShader(const ShaderDesc& desc) override
            {
                std::vector<std::string> defineStorage;
                std::vector<D3D_SHADER_MACRO> macros;
                for (const auto& [name, value] : desc.defines)
                {
                    defineStorage.push_back(name);
                    defineStorage.push_back(value.empty() ? "1" : value);
                }
                for (size_t i = 0; i < defineStorage.size(); i += 2)
                {
                    macros.push_back({ defineStorage[i].c_str(), defineStorage[i + 1].c_str() });
                }
                macros.push_back({ nullptr, nullptr });

                UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_OPTIMIZATION_LEVEL3;
                const char* target = desc.stage == ShaderStage::Vertex ? VertexShaderTarget : PixelShaderTarget;
                ComPtr<ID3DBlob> bytecode;
                ComPtr<ID3DBlob> errors;
                HRESULT result = D3DCompile(desc.source.data(), desc.source.size(), desc.debugName.c_str(), macros.data(), nullptr, desc.entryPoint.c_str(), target, flags, 0, &bytecode, &errors);
                if (FAILED(result))
                {
                    if (!errors)
                    {
                        return Core::Error(Core::EngineText::D3DCompileFailed);
                    }
                    return Core::Error(Core::EngineText::D3DCompileError, std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()));
                }
                auto shader = std::make_unique<DX11Shader>(desc.stage, bytecode);
                if (desc.stage == ShaderStage::Vertex)
                {
                    m_device->CreateVertexShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &shader->vertex);
                }
                else
                {
                    m_device->CreatePixelShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &shader->pixel);
                }
                return shader;
            }

            Core::Result<std::unique_ptr<Pipeline>> CreatePipeline(const PipelineDesc& desc) override
            {
                auto pipeline = std::make_unique<DX11Pipeline>(desc);
                auto* vertex = static_cast<DX11Shader*>(desc.vertexShader);
                if (!desc.vertexLayout.attributes.empty() && vertex)
                {
                    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
                    for (const VertexAttribute& attribute : desc.vertexLayout.attributes)
                    {
                        D3D11_INPUT_ELEMENT_DESC element{};
                        element.SemanticName = VertexSemanticName(attribute.semantic);
                        element.SemanticIndex = attribute.semanticIndex;
                        element.Format = ToDXGI(attribute.format);
                        element.InputSlot = attribute.bufferSlot;
                        element.AlignedByteOffset = attribute.offset;
                        bool instanced = desc.vertexLayout.perInstance[attribute.bufferSlot];
                        element.InputSlotClass = instanced ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
                        element.InstanceDataStepRate = instanced ? 1 : 0;
                        elements.push_back(element);
                    }
                    if (FAILED(m_device->CreateInputLayout(elements.data(), static_cast<UINT>(elements.size()), vertex->bytecode->GetBufferPointer(), vertex->bytecode->GetBufferSize(), &pipeline->inputLayout)))
                    {
                        return Core::Error(Core::EngineText::InputLayoutFailed, desc.debugName);
                    }
                }

                D3D11_RASTERIZER_DESC raster{};
                raster.FillMode = desc.rasterizer.wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
                raster.CullMode = desc.rasterizer.cullMode == CullMode::None ? D3D11_CULL_NONE : (desc.rasterizer.cullMode == CullMode::Front ? D3D11_CULL_FRONT : D3D11_CULL_BACK);
                raster.FrontCounterClockwise = desc.rasterizer.frontCounterClockwise;
                raster.DepthBias = desc.rasterizer.depthBias;
                raster.SlopeScaledDepthBias = desc.rasterizer.slopeScaledDepthBias;
                raster.DepthBiasClamp = desc.rasterizer.depthBiasClamp;
                raster.DepthClipEnable = desc.rasterizer.depthClip;
                raster.ScissorEnable = TRUE;
                m_device->CreateRasterizerState(&raster, &pipeline->rasterizer);

                D3D11_DEPTH_STENCIL_DESC depth{};
                depth.DepthEnable = desc.depthStencil.depthTest || desc.depthStencil.depthWrite;
                depth.DepthWriteMask = desc.depthStencil.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
                depth.DepthFunc = desc.depthStencil.depthTest ? ToD3D(desc.depthStencil.depthCompare) : D3D11_COMPARISON_ALWAYS;
                m_device->CreateDepthStencilState(&depth, &pipeline->depthStencil);

                D3D11_BLEND_DESC blend{};
                blend.IndependentBlendEnable = TRUE;
                for (uint32_t i = 0; i < MaxColorTargets; ++i)
                {
                    const BlendState& state = desc.blend[i];
                    D3D11_RENDER_TARGET_BLEND_DESC& target = blend.RenderTarget[i];
                    target.BlendEnable = state.enabled;
                    target.SrcBlend = ToD3D(state.sourceColor);
                    target.DestBlend = ToD3D(state.destinationColor);
                    target.BlendOp = ToD3D(state.colorOperation);
                    target.SrcBlendAlpha = ToD3D(state.sourceAlpha);
                    target.DestBlendAlpha = ToD3D(state.destinationAlpha);
                    target.BlendOpAlpha = ToD3D(state.alphaOperation);
                    target.RenderTargetWriteMask = state.writeMask;
                }
                m_device->CreateBlendState(&blend, &pipeline->blend);
                return pipeline;
            }

            std::unique_ptr<Swapchain> CreateSwapchain(Platform::Window& window, PresentMode presentMode) override
            {
                return std::make_unique<DX11Swapchain>(m_device.Get(), m_factory.Get(), window, presentMode);
            }

            CommandList& BeginFrame() override
            {
                m_inFrame = true;
                m_commandList.ResetState();
                return m_commandList;
            }

            void EndFrame(const std::vector<Swapchain*>& present) override
            {
                for (Swapchain* swapchain : present)
                {
                    if (swapchain)
                    {
                        static_cast<DX11Swapchain*>(swapchain)->Present();
                    }
                }
                m_inFrame = false;
            }

            bool ReadTexture(Texture* texture, std::vector<uint8_t>& pixels) override
            {
                auto* dx = static_cast<DX11Texture*>(texture);
                uint32_t bytesPerPixel = FormatBytesPerPixel(texture->GetFormat());
                if (!dx->texture || bytesPerPixel == 0)
                {
                    return false;
                }
                D3D11_TEXTURE2D_DESC desc{};
                dx->texture->GetDesc(&desc);
                desc.Usage = D3D11_USAGE_STAGING;
                desc.BindFlags = 0;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                desc.MiscFlags = 0;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                ComPtr<ID3D11Texture2D> staging;
                if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &staging)))
                {
                    return false;
                }
                m_context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, dx->texture.Get(), 0, nullptr);
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (FAILED(m_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
                {
                    return false;
                }
                size_t rowPitch = static_cast<size_t>(texture->GetWidth()) * bytesPerPixel;
                pixels.resize(rowPitch * texture->GetHeight());
                for (uint32_t y = 0; y < texture->GetHeight(); ++y)
                {
                    std::memcpy(pixels.data() + y * rowPitch, static_cast<uint8_t*>(mapped.pData) + y * mapped.RowPitch, rowPitch);
                }
                m_context->Unmap(staging.Get(), 0);
                return true;
            }

            void WaitIdle() override
            {
                m_context->Flush();
            }

            bool IsInFrame() const override { return m_inFrame; }

        private:
            ComPtr<ID3D11Device> m_device;
            ComPtr<ID3D11DeviceContext> m_context;
            ComPtr<IDXGIFactory2> m_factory;
            DX11CommandList m_commandList;
            std::string m_name;
            bool m_inFrame = false;
        };
    }

    std::unique_ptr<Device> CreateDX11Device()
    {
        auto device = std::make_unique<DX11Device>();
        if (!device->Initialize())
        {
            Core::Log::Error(Core::Text(Core::EngineText::DX11DeviceFailed));
            return nullptr;
        }
        return device;
    }
}
