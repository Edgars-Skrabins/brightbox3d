#include "RHI/Device.h"

#include "Core/Log.h"
#include "Platform/Window.h"
#include "RHI/ShaderCompiler.h"

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace BrightBox3D::RHI
{
    namespace
    {
        using Microsoft::WRL::ComPtr;

        constexpr size_t TransientRingSize = 16 * 1024 * 1024;

        DXGI_FORMAT ToDXGI(Format format)
        {
            switch (format)
            {
            case Format::R8Unorm:
                return DXGI_FORMAT_R8_UNORM;
            case Format::RG8Unorm:
                return DXGI_FORMAT_R8G8_UNORM;
            case Format::RGBA8Unorm:
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            case Format::RGBA8Srgb:
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case Format::BGRA8Unorm:
                return DXGI_FORMAT_B8G8R8A8_UNORM;
            case Format::BGRA8Srgb:
                return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            case Format::R16Float:
                return DXGI_FORMAT_R16_FLOAT;
            case Format::RG16Float:
                return DXGI_FORMAT_R16G16_FLOAT;
            case Format::RGBA16Float:
                return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case Format::R32Float:
                return DXGI_FORMAT_R32_FLOAT;
            case Format::RG32Float:
                return DXGI_FORMAT_R32G32_FLOAT;
            case Format::RGB32Float:
                return DXGI_FORMAT_R32G32B32_FLOAT;
            case Format::RGBA32Float:
                return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case Format::R32Uint:
                return DXGI_FORMAT_R32_UINT;
            case Format::RG11B10Float:
                return DXGI_FORMAT_R11G11B10_FLOAT;
            case Format::RGB10A2Unorm:
                return DXGI_FORMAT_R10G10B10A2_UNORM;
            case Format::Depth32Float:
                return DXGI_FORMAT_D32_FLOAT;
            case Format::Depth24Stencil8:
                return DXGI_FORMAT_D24_UNORM_S8_UINT;
            default:
                return DXGI_FORMAT_UNKNOWN;
            }
        }

        DXGI_FORMAT TypelessDepth(Format format)
        {
            return format == Format::Depth32Float ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_R24G8_TYPELESS;
        }

        DXGI_FORMAT DepthShaderView(Format format)
        {
            return format == Format::Depth32Float ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        }

        D3D11_COMPARISON_FUNC ToD3D(CompareFunction function)
        {
            switch (function)
            {
            case CompareFunction::Never:
                return D3D11_COMPARISON_NEVER;
            case CompareFunction::Less:
                return D3D11_COMPARISON_LESS;
            case CompareFunction::Equal:
                return D3D11_COMPARISON_EQUAL;
            case CompareFunction::LessEqual:
                return D3D11_COMPARISON_LESS_EQUAL;
            case CompareFunction::Greater:
                return D3D11_COMPARISON_GREATER;
            case CompareFunction::NotEqual:
                return D3D11_COMPARISON_NOT_EQUAL;
            case CompareFunction::GreaterEqual:
                return D3D11_COMPARISON_GREATER_EQUAL;
            default:
                return D3D11_COMPARISON_ALWAYS;
            }
        }

        D3D11_BLEND ToD3D(BlendFactor factor)
        {
            switch (factor)
            {
            case BlendFactor::Zero:
                return D3D11_BLEND_ZERO;
            case BlendFactor::One:
                return D3D11_BLEND_ONE;
            case BlendFactor::SourceColor:
                return D3D11_BLEND_SRC_COLOR;
            case BlendFactor::InverseSourceColor:
                return D3D11_BLEND_INV_SRC_COLOR;
            case BlendFactor::SourceAlpha:
                return D3D11_BLEND_SRC_ALPHA;
            case BlendFactor::InverseSourceAlpha:
                return D3D11_BLEND_INV_SRC_ALPHA;
            case BlendFactor::DestinationColor:
                return D3D11_BLEND_DEST_COLOR;
            case BlendFactor::InverseDestinationColor:
                return D3D11_BLEND_INV_DEST_COLOR;
            case BlendFactor::DestinationAlpha:
                return D3D11_BLEND_DEST_ALPHA;
            default:
                return D3D11_BLEND_INV_DEST_ALPHA;
            }
        }

        D3D11_BLEND_OP ToD3D(BlendOperation operation)
        {
            switch (operation)
            {
            case BlendOperation::Subtract:
                return D3D11_BLEND_OP_SUBTRACT;
            case BlendOperation::ReverseSubtract:
                return D3D11_BLEND_OP_REV_SUBTRACT;
            case BlendOperation::Min:
                return D3D11_BLEND_OP_MIN;
            case BlendOperation::Max:
                return D3D11_BLEND_OP_MAX;
            default:
                return D3D11_BLEND_OP_ADD;
            }
        }

        D3D11_TEXTURE_ADDRESS_MODE ToD3D(AddressMode mode)
        {
            switch (mode)
            {
            case AddressMode::Clamp:
                return D3D11_TEXTURE_ADDRESS_CLAMP;
            case AddressMode::Mirror:
                return D3D11_TEXTURE_ADDRESS_MIRROR;
            case AddressMode::Border:
                return D3D11_TEXTURE_ADDRESS_BORDER;
            default:
                return D3D11_TEXTURE_ADDRESS_WRAP;
            }
        }

        D3D11_PRIMITIVE_TOPOLOGY ToD3D(PrimitiveTopology topology)
        {
            switch (topology)
            {
            case PrimitiveTopology::TriangleStrip:
                return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            case PrimitiveTopology::LineList:
                return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
            case PrimitiveTopology::LineStrip:
                return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
            case PrimitiveTopology::PointList:
                return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
            default:
                return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            }
        }

        std::wstring Widen(const std::string& text)
        {
            if (text.empty())
            {
                return {};
            }
            int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
            std::wstring result(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
            return result;
        }

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

            ID3D11RenderTargetView* GetRenderView(ID3D11Device* device, uint32_t mip, uint32_t layer)
            {
                uint32_t key = (mip << 16) | layer;
                auto it = renderViews.find(key);
                if (it != renderViews.end())
                {
                    return it->second.Get();
                }
                D3D11_RENDER_TARGET_VIEW_DESC desc{};
                desc.Format = ToDXGI(m_desc.format);
                if (m_desc.type == TextureType::Texture2D)
                {
                    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                    desc.Texture2D.MipSlice = mip;
                }
                else
                {
                    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                    desc.Texture2DArray.MipSlice = mip;
                    desc.Texture2DArray.FirstArraySlice = layer;
                    desc.Texture2DArray.ArraySize = 1;
                }
                ComPtr<ID3D11RenderTargetView> view;
                device->CreateRenderTargetView(texture.Get(), &desc, &view);
                renderViews[key] = view;
                return view.Get();
            }

            ID3D11DepthStencilView* GetDepthView(ID3D11Device* device, uint32_t layer)
            {
                auto it = depthViews.find(layer);
                if (it != depthViews.end())
                {
                    return it->second.Get();
                }
                D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
                desc.Format = ToDXGI(m_desc.format);
                if (m_desc.type == TextureType::Texture2D)
                {
                    desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                }
                else
                {
                    desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                    desc.Texture2DArray.FirstArraySlice = layer;
                    desc.Texture2DArray.ArraySize = 1;
                }
                ComPtr<ID3D11DepthStencilView> view;
                device->CreateDepthStencilView(texture.Get(), &desc, &view);
                depthViews[layer] = view;
                return view.Get();
            }
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

        class DX11Swapchain : public Swapchain
        {
        public:
            DX11Swapchain(ID3D11Device* device, IDXGIFactory2* factory, Platform::Window& window, bool vsync) : m_device(device), m_window(window), m_vsync(vsync)
            {
                Math::Vector2 size = window.GetFramebufferSize();
                m_width = std::max<uint32_t>(static_cast<uint32_t>(size.x), 1);
                m_height = std::max<uint32_t>(static_cast<uint32_t>(size.y), 1);
                DXGI_SWAP_CHAIN_DESC1 desc{};
                desc.Width = m_width;
                desc.Height = m_height;
                desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                desc.BufferCount = 2;
                desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
                HWND hwnd = static_cast<HWND>(window.GetNativeHandle());
                HRESULT result = factory->CreateSwapChainForHwnd(device, hwnd, &desc, nullptr, nullptr, &m_swapchain);
                if (FAILED(result))
                {
                    desc.Flags = 0;
                    m_allowTearing = false;
                    factory->CreateSwapChainForHwnd(device, hwnd, &desc, nullptr, nullptr, &m_swapchain);
                }
                factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
                TextureDesc textureDesc;
                textureDesc.width = m_width;
                textureDesc.height = m_height;
                textureDesc.format = Format::BGRA8Unorm;
                textureDesc.usage = TextureUsageRenderTarget;
                textureDesc.debugName = "Swapchain";
                m_texture = std::make_unique<DX11Texture>(textureDesc);
                AcquireBackbuffer();
            }

            Texture* AcquireNextTexture() override
            {
                Math::Vector2 size = m_window.GetFramebufferSize();
                if (size.x < 1.0f || size.y < 1.0f)
                {
                    return nullptr;
                }
                if (static_cast<uint32_t>(size.x) != m_width || static_cast<uint32_t>(size.y) != m_height)
                {
                    Resize(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
                }
                m_acquired = true;
                return m_texture.get();
            }

            void Resize(uint32_t width, uint32_t height) override
            {
                if (width == 0 || height == 0 || !m_swapchain)
                {
                    return;
                }
                m_texture->texture.Reset();
                m_texture->renderViews.clear();
                m_width = width;
                m_height = height;
                m_swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
                m_texture->SetSize(width, height);
                AcquireBackbuffer();
            }

            uint32_t GetWidth() const override { return m_width; }
            uint32_t GetHeight() const override { return m_height; }
            Format GetFormat() const override { return Format::BGRA8Unorm; }
            void SetVSync(bool enabled) override { m_vsync = enabled; }

            void Present()
            {
                if (!m_acquired || !m_swapchain)
                {
                    return;
                }
                m_acquired = false;
                UINT flags = !m_vsync && m_allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
                m_swapchain->Present(m_vsync ? 1 : 0, flags);
            }

        private:
            void AcquireBackbuffer()
            {
                if (m_swapchain)
                {
                    m_swapchain->GetBuffer(0, IID_PPV_ARGS(&m_texture->texture));
                }
            }

            ID3D11Device* m_device;
            Platform::Window& m_window;
            ComPtr<IDXGISwapChain1> m_swapchain;
            std::unique_ptr<DX11Texture> m_texture;
            uint32_t m_width = 0;
            uint32_t m_height = 0;
            bool m_vsync = true;
            bool m_allowTearing = true;
            bool m_acquired = false;
        };

        class DynamicRing
        {
        public:
            void Initialize(ID3D11Device* device, D3D11_BIND_FLAG bind, size_t size)
            {
                D3D11_BUFFER_DESC desc{};
                desc.ByteWidth = static_cast<UINT>(size);
                desc.Usage = D3D11_USAGE_DYNAMIC;
                desc.BindFlags = bind;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                device->CreateBuffer(&desc, nullptr, &m_buffer);
                m_size = size;
                m_offset = size;
            }

            ID3D11Buffer* Write(ID3D11DeviceContext* context, const void* data, size_t size, size_t alignment, UINT& offset)
            {
                size_t aligned = (m_offset + alignment - 1) / alignment * alignment;
                D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
                if (aligned + size > m_size)
                {
                    aligned = 0;
                    mode = D3D11_MAP_WRITE_DISCARD;
                }
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (FAILED(context->Map(m_buffer.Get(), 0, mode, 0, &mapped)))
                {
                    offset = 0;
                    return nullptr;
                }
                std::memcpy(static_cast<uint8_t*>(mapped.pData) + aligned, data, size);
                context->Unmap(m_buffer.Get(), 0);
                m_offset = aligned + size;
                offset = static_cast<UINT>(aligned);
                return m_buffer.Get();
            }

        private:
            ComPtr<ID3D11Buffer> m_buffer;
            size_t m_size = 0;
            size_t m_offset = 0;
        };

        class ConstantPool
        {
        public:
            void Initialize(ID3D11Device* device)
            {
                m_device = device;
            }

            ID3D11Buffer* Write(ID3D11DeviceContext* context, uint32_t slot, const void* data, size_t size)
            {
                size_t bucket = 256;
                while (bucket < size)
                {
                    bucket *= 2;
                }
                uint64_t key = (static_cast<uint64_t>(slot) << 32) | bucket;
                ComPtr<ID3D11Buffer>& buffer = m_buffers[key];
                if (!buffer)
                {
                    D3D11_BUFFER_DESC desc{};
                    desc.ByteWidth = static_cast<UINT>(bucket);
                    desc.Usage = D3D11_USAGE_DYNAMIC;
                    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                    m_device->CreateBuffer(&desc, nullptr, &buffer);
                }
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (SUCCEEDED(context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                {
                    std::memcpy(mapped.pData, data, size);
                    context->Unmap(buffer.Get(), 0);
                }
                return buffer.Get();
            }

        private:
            ID3D11Device* m_device = nullptr;
            std::unordered_map<uint64_t, ComPtr<ID3D11Buffer>> m_buffers;
        };

        class DX11CommandList : public CommandList
        {
        public:
            void Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
            {
                m_device = device;
                m_context = context;
                m_context.As(&m_annotation);
                m_vertexRing.Initialize(device, D3D11_BIND_VERTEX_BUFFER, TransientRingSize);
                m_indexRing.Initialize(device, D3D11_BIND_INDEX_BUFFER, TransientRingSize / 2);
                m_constants.Initialize(device);
            }

            void BeginRenderPass(const RenderPassDesc& desc) override
            {
                ID3D11ShaderResourceView* nullViews[MaxTextureSlots] = {};
                m_context->PSSetShaderResources(0, MaxTextureSlots, nullViews);
                m_context->VSSetShaderResources(0, MaxTextureSlots, nullViews);

                ID3D11RenderTargetView* targets[MaxColorTargets] = {};
                UINT count = 0;
                uint32_t width = 0;
                uint32_t height = 0;
                for (size_t i = 0; i < desc.colors.size() && i < MaxColorTargets; ++i)
                {
                    auto* texture = static_cast<DX11Texture*>(desc.colors[i].texture);
                    if (!texture)
                    {
                        continue;
                    }
                    targets[i] = texture->GetRenderView(m_device, desc.colors[i].mipLevel, desc.colors[i].layer);
                    count = static_cast<UINT>(i + 1);
                    if (desc.colors[i].load == LoadAction::Clear)
                    {
                        m_context->ClearRenderTargetView(targets[i], desc.colors[i].clearColor.data());
                    }
                    width = std::max<uint32_t>(texture->GetWidth() >> desc.colors[i].mipLevel, 1);
                    height = std::max<uint32_t>(texture->GetHeight() >> desc.colors[i].mipLevel, 1);
                }
                ID3D11DepthStencilView* depthView = nullptr;
                if (auto* depth = static_cast<DX11Texture*>(desc.depth.texture))
                {
                    depthView = depth->GetDepthView(m_device, desc.depth.layer);
                    if (desc.depth.load == LoadAction::Clear)
                    {
                        m_context->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, desc.depth.clearDepth, 0);
                    }
                    if (width == 0)
                    {
                        width = depth->GetWidth();
                        height = depth->GetHeight();
                    }
                }
                m_context->OMSetRenderTargets(count, targets, depthView);
                m_passWidth = width;
                m_passHeight = height;
                m_pipeline = nullptr;
                Viewport viewport;
                viewport.width = static_cast<float>(width);
                viewport.height = static_cast<float>(height);
                SetViewport(viewport);
                if (m_annotation && !desc.debugName.empty())
                {
                    m_annotation->BeginEvent(Widen(desc.debugName).c_str());
                    m_inPassEvent = true;
                }
            }

            void EndRenderPass() override
            {
                m_context->OMSetRenderTargets(0, nullptr, nullptr);
                if (m_inPassEvent && m_annotation)
                {
                    m_annotation->EndEvent();
                    m_inPassEvent = false;
                }
            }

            void SetPipeline(Pipeline* pipeline) override
            {
                auto* dx = static_cast<DX11Pipeline*>(pipeline);
                if (!dx || dx == m_pipeline)
                {
                    return;
                }
                m_pipeline = dx;
                const PipelineDesc& desc = dx->GetDesc();
                auto* vertex = static_cast<DX11Shader*>(desc.vertexShader);
                auto* pixel = static_cast<DX11Shader*>(desc.pixelShader);
                m_context->VSSetShader(vertex ? vertex->vertex.Get() : nullptr, nullptr, 0);
                m_context->PSSetShader(pixel ? pixel->pixel.Get() : nullptr, nullptr, 0);
                m_context->IASetInputLayout(dx->inputLayout.Get());
                m_context->IASetPrimitiveTopology(ToD3D(desc.topology));
                m_context->RSSetState(dx->rasterizer.Get());
                m_context->OMSetDepthStencilState(dx->depthStencil.Get(), 0);
                float blendFactor[4] = { 1, 1, 1, 1 };
                m_context->OMSetBlendState(dx->blend.Get(), blendFactor, 0xFFFFFFFF);
                if (!desc.rasterizer.scissor)
                {
                    D3D11_RECT rect = { 0, 0, static_cast<LONG>(m_passWidth), static_cast<LONG>(m_passHeight) };
                    m_context->RSSetScissorRects(1, &rect);
                }
            }

            void SetViewport(const Viewport& viewport) override
            {
                D3D11_VIEWPORT dx = { viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth };
                m_context->RSSetViewports(1, &dx);
            }

            void SetScissor(const ScissorRect& scissor) override
            {
                D3D11_RECT rect = { scissor.x, scissor.y, scissor.x + scissor.width, scissor.y + scissor.height };
                m_context->RSSetScissorRects(1, &rect);
            }

            void SetVertexBuffer(uint32_t slot, Buffer* buffer, size_t offset) override
            {
                ID3D11Buffer* dx = buffer ? static_cast<DX11Buffer*>(buffer)->Get() : nullptr;
                UINT stride = m_pipeline ? m_pipeline->GetDesc().vertexLayout.strides[slot] : 0;
                UINT dxOffset = static_cast<UINT>(offset);
                m_context->IASetVertexBuffers(slot, 1, &dx, &stride, &dxOffset);
            }

            void SetIndexBuffer(Buffer* buffer, IndexFormat format, size_t offset) override
            {
                ID3D11Buffer* dx = buffer ? static_cast<DX11Buffer*>(buffer)->Get() : nullptr;
                m_indexSize = format == IndexFormat::UInt16 ? 2 : 4;
                m_context->IASetIndexBuffer(dx, format == IndexFormat::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, static_cast<UINT>(offset));
            }

            void SetVertexData(uint32_t slot, const void* data, size_t size) override
            {
                UINT stride = m_pipeline ? m_pipeline->GetDesc().vertexLayout.strides[slot] : 4;
                UINT offset = 0;
                ID3D11Buffer* buffer = m_vertexRing.Write(m_context.Get(), data, size, std::max<UINT>(stride, 4), offset);
                m_context->IASetVertexBuffers(slot, 1, &buffer, &stride, &offset);
            }

            void SetIndexData(const void* data, size_t size, IndexFormat format) override
            {
                UINT offset = 0;
                ID3D11Buffer* buffer = m_indexRing.Write(m_context.Get(), data, size, 4, offset);
                m_indexSize = format == IndexFormat::UInt16 ? 2 : 4;
                m_context->IASetIndexBuffer(buffer, format == IndexFormat::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, offset);
            }

            void SetConstants(uint32_t slot, const void* data, size_t size) override
            {
                ID3D11Buffer* buffer = m_constants.Write(m_context.Get(), slot, data, size);
                m_context->VSSetConstantBuffers(slot, 1, &buffer);
                m_context->PSSetConstantBuffers(slot, 1, &buffer);
            }

            void SetConstantBuffer(uint32_t slot, Buffer* buffer) override
            {
                ID3D11Buffer* dx = buffer ? static_cast<DX11Buffer*>(buffer)->Get() : nullptr;
                m_context->VSSetConstantBuffers(slot, 1, &dx);
                m_context->PSSetConstantBuffers(slot, 1, &dx);
            }

            void SetTexture(uint32_t slot, Texture* texture) override
            {
                ID3D11ShaderResourceView* view = texture ? static_cast<DX11Texture*>(texture)->shaderView.Get() : nullptr;
                m_context->VSSetShaderResources(slot, 1, &view);
                m_context->PSSetShaderResources(slot, 1, &view);
            }

            void SetSampler(uint32_t slot, Sampler* sampler) override
            {
                ID3D11SamplerState* state = sampler ? static_cast<DX11Sampler*>(sampler)->Get() : nullptr;
                m_context->VSSetSamplers(slot, 1, &state);
                m_context->PSSetSamplers(slot, 1, &state);
            }

            void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override
            {
                if (vertexCount == 0)
                {
                    return;
                }
                if (instanceCount > 1 || firstInstance > 0)
                {
                    m_context->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
                }
                else
                {
                    m_context->Draw(vertexCount, firstVertex);
                }
            }

            void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) override
            {
                if (indexCount == 0)
                {
                    return;
                }
                if (instanceCount > 1 || firstInstance > 0)
                {
                    m_context->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
                }
                else
                {
                    m_context->DrawIndexed(indexCount, firstIndex, baseVertex);
                }
            }

            void CopyTexture(Texture* source, Texture* destination) override
            {
                m_context->CopyResource(static_cast<DX11Texture*>(destination)->texture.Get(), static_cast<DX11Texture*>(source)->texture.Get());
            }

            void GenerateMips(Texture* texture) override
            {
                auto* dx = static_cast<DX11Texture*>(texture);
                if (dx->shaderView)
                {
                    m_context->GenerateMips(dx->shaderView.Get());
                }
            }

            void PushDebugGroup(const char* name) override
            {
                if (m_annotation)
                {
                    m_annotation->BeginEvent(Widen(name).c_str());
                }
            }

            void PopDebugGroup() override
            {
                if (m_annotation)
                {
                    m_annotation->EndEvent();
                }
            }

            void ResetState()
            {
                m_pipeline = nullptr;
            }

        private:
            ID3D11Device* m_device = nullptr;
            ComPtr<ID3D11DeviceContext> m_context;
            ComPtr<ID3DUserDefinedAnnotation> m_annotation;
            DX11Pipeline* m_pipeline = nullptr;
            DynamicRing m_vertexRing;
            DynamicRing m_indexRing;
            ConstantPool m_constants;
            uint32_t m_passWidth = 0;
            uint32_t m_passHeight = 0;
            uint32_t m_indexSize = 4;
            bool m_inPassEvent = false;
        };

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
                    Core::Log::Error("DX11: failed to create buffer '" + desc.debugName + "'");
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
                    Core::Log::Error("DX11: failed to create texture '" + desc.debugName + "'");
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

            std::unique_ptr<Shader> CreateShader(const ShaderDesc& desc, std::string& error) override
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
                const char* target = desc.stage == ShaderStage::Vertex ? "vs_5_0" : "ps_5_0";
                ComPtr<ID3DBlob> bytecode;
                ComPtr<ID3DBlob> errors;
                HRESULT result = D3DCompile(desc.source.data(), desc.source.size(), desc.debugName.c_str(), macros.data(), nullptr, desc.entryPoint.c_str(), target, flags, 0, &bytecode, &errors);
                if (FAILED(result))
                {
                    error = errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()) : "D3DCompile failed";
                    return nullptr;
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

            std::unique_ptr<Pipeline> CreatePipeline(const PipelineDesc& desc, std::string& error) override
            {
                auto pipeline = std::make_unique<DX11Pipeline>(desc);
                auto* vertex = static_cast<DX11Shader*>(desc.vertexShader);
                if (!desc.vertexLayout.attributes.empty() && vertex)
                {
                    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
                    for (const VertexAttribute& attribute : desc.vertexLayout.attributes)
                    {
                        D3D11_INPUT_ELEMENT_DESC element{};
                        element.SemanticName = attribute.semantic.c_str();
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
                        error = "input layout creation failed for '" + desc.debugName + "'";
                        return nullptr;
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

            std::unique_ptr<Swapchain> CreateSwapchain(Platform::Window& window, bool vsync) override
            {
                return std::make_unique<DX11Swapchain>(m_device.Get(), m_factory.Get(), window, vsync);
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
            Core::Log::Error("DirectX 11 device creation failed");
            return nullptr;
        }
        return device;
    }
}
