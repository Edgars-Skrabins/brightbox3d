#include "RHI/Device.h"

#include "Core/Log.h"
#include "Platform/Window.h"
#include "RHI/ShaderCompiler.h"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace BrightBox3D::RHI
{
    namespace
    {
        constexpr uint32_t FramesInFlight = 3;
        constexpr uint32_t VertexBufferBase = 16;
        constexpr size_t TransientPageSize = 8 * 1024 * 1024;
        constexpr size_t TransientAlignment = 256;

        MTLPixelFormat ToMetal(Format format)
        {
            switch (format)
            {
            case Format::R8Unorm:
                return MTLPixelFormatR8Unorm;
            case Format::RG8Unorm:
                return MTLPixelFormatRG8Unorm;
            case Format::RGBA8Unorm:
                return MTLPixelFormatRGBA8Unorm;
            case Format::RGBA8Srgb:
                return MTLPixelFormatRGBA8Unorm_sRGB;
            case Format::BGRA8Unorm:
                return MTLPixelFormatBGRA8Unorm;
            case Format::BGRA8Srgb:
                return MTLPixelFormatBGRA8Unorm_sRGB;
            case Format::R16Float:
                return MTLPixelFormatR16Float;
            case Format::RG16Float:
                return MTLPixelFormatRG16Float;
            case Format::RGBA16Float:
                return MTLPixelFormatRGBA16Float;
            case Format::R32Float:
                return MTLPixelFormatR32Float;
            case Format::RG32Float:
                return MTLPixelFormatRG32Float;
            case Format::RGBA32Float:
                return MTLPixelFormatRGBA32Float;
            case Format::R32Uint:
                return MTLPixelFormatR32Uint;
            case Format::RG11B10Float:
                return MTLPixelFormatRG11B10Float;
            case Format::RGB10A2Unorm:
                return MTLPixelFormatRGB10A2Unorm;
            case Format::Depth32Float:
                return MTLPixelFormatDepth32Float;
            case Format::Depth24Stencil8:
                return MTLPixelFormatDepth32Float_Stencil8;
            default:
                return MTLPixelFormatInvalid;
            }
        }

        MTLVertexFormat ToMetalVertex(Format format)
        {
            switch (format)
            {
            case Format::R32Float:
                return MTLVertexFormatFloat;
            case Format::RG32Float:
                return MTLVertexFormatFloat2;
            case Format::RGB32Float:
                return MTLVertexFormatFloat3;
            case Format::RGBA32Float:
                return MTLVertexFormatFloat4;
            case Format::RGBA8Unorm:
                return MTLVertexFormatUChar4Normalized;
            case Format::R32Uint:
                return MTLVertexFormatUInt;
            case Format::RG16Float:
                return MTLVertexFormatHalf2;
            case Format::RGBA16Float:
                return MTLVertexFormatHalf4;
            default:
                return MTLVertexFormatInvalid;
            }
        }

        MTLCompareFunction ToMetal(CompareFunction function)
        {
            switch (function)
            {
            case CompareFunction::Never:
                return MTLCompareFunctionNever;
            case CompareFunction::Less:
                return MTLCompareFunctionLess;
            case CompareFunction::Equal:
                return MTLCompareFunctionEqual;
            case CompareFunction::LessEqual:
                return MTLCompareFunctionLessEqual;
            case CompareFunction::Greater:
                return MTLCompareFunctionGreater;
            case CompareFunction::NotEqual:
                return MTLCompareFunctionNotEqual;
            case CompareFunction::GreaterEqual:
                return MTLCompareFunctionGreaterEqual;
            default:
                return MTLCompareFunctionAlways;
            }
        }

        MTLBlendFactor ToMetal(BlendFactor factor)
        {
            switch (factor)
            {
            case BlendFactor::Zero:
                return MTLBlendFactorZero;
            case BlendFactor::One:
                return MTLBlendFactorOne;
            case BlendFactor::SourceColor:
                return MTLBlendFactorSourceColor;
            case BlendFactor::InverseSourceColor:
                return MTLBlendFactorOneMinusSourceColor;
            case BlendFactor::SourceAlpha:
                return MTLBlendFactorSourceAlpha;
            case BlendFactor::InverseSourceAlpha:
                return MTLBlendFactorOneMinusSourceAlpha;
            case BlendFactor::DestinationColor:
                return MTLBlendFactorDestinationColor;
            case BlendFactor::InverseDestinationColor:
                return MTLBlendFactorOneMinusDestinationColor;
            case BlendFactor::DestinationAlpha:
                return MTLBlendFactorDestinationAlpha;
            default:
                return MTLBlendFactorOneMinusDestinationAlpha;
            }
        }

        MTLBlendOperation ToMetal(BlendOperation operation)
        {
            switch (operation)
            {
            case BlendOperation::Subtract:
                return MTLBlendOperationSubtract;
            case BlendOperation::ReverseSubtract:
                return MTLBlendOperationReverseSubtract;
            case BlendOperation::Min:
                return MTLBlendOperationMin;
            case BlendOperation::Max:
                return MTLBlendOperationMax;
            default:
                return MTLBlendOperationAdd;
            }
        }

        MTLSamplerAddressMode ToMetal(AddressMode mode)
        {
            switch (mode)
            {
            case AddressMode::Clamp:
                return MTLSamplerAddressModeClampToEdge;
            case AddressMode::Mirror:
                return MTLSamplerAddressModeMirrorRepeat;
            case AddressMode::Border:
                return MTLSamplerAddressModeClampToBorderColor;
            default:
                return MTLSamplerAddressModeRepeat;
            }
        }

        MTLPrimitiveType ToMetal(PrimitiveTopology topology)
        {
            switch (topology)
            {
            case PrimitiveTopology::TriangleStrip:
                return MTLPrimitiveTypeTriangleStrip;
            case PrimitiveTopology::LineList:
                return MTLPrimitiveTypeLine;
            case PrimitiveTopology::LineStrip:
                return MTLPrimitiveTypeLineStrip;
            case PrimitiveTopology::PointList:
                return MTLPrimitiveTypePoint;
            default:
                return MTLPrimitiveTypeTriangle;
            }
        }

        class MetalBuffer : public Buffer
        {
        public:
            MetalBuffer(id<MTLDevice> device, const BufferDesc& desc, const void* initialData) : Buffer(desc)
            {
                size_t size = std::max<size_t>(desc.size, 16);
                uint32_t copies = desc.dynamic ? FramesInFlight : 1;
                for (uint32_t i = 0; i < copies; ++i)
                {
                    id<MTLBuffer> buffer = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
                    if (initialData)
                    {
                        std::memcpy([buffer contents], initialData, desc.size);
                    }
                    if (!desc.debugName.empty())
                    {
                        buffer.label = [NSString stringWithUTF8String:desc.debugName.c_str()];
                    }
                    m_buffers[i] = buffer;
                }
            }

            id<MTLBuffer> Get(uint32_t frame) const { return m_desc.dynamic ? m_buffers[frame % FramesInFlight] : m_buffers[0]; }

        private:
            id<MTLBuffer> m_buffers[FramesInFlight];
        };

        class MetalTexture : public Texture
        {
        public:
            MetalTexture(id<MTLDevice> device, const TextureDesc& desc) : Texture(desc)
            {
                MTLTextureDescriptor* descriptor = [[MTLTextureDescriptor alloc] init];
                switch (desc.type)
                {
                case TextureType::TextureCube:
                    descriptor.textureType = MTLTextureTypeCube;
                    break;
                case TextureType::Texture2DArray:
                    descriptor.textureType = MTLTextureType2DArray;
                    descriptor.arrayLength = desc.layers;
                    break;
                default:
                    descriptor.textureType = MTLTextureType2D;
                    break;
                }
                descriptor.pixelFormat = ToMetal(desc.format);
                descriptor.width = desc.width;
                descriptor.height = desc.height;
                descriptor.mipmapLevelCount = std::max<uint32_t>(desc.mipLevels, 1);
                MTLTextureUsage usage = 0;
                if (desc.usage & TextureUsageSampled)
                {
                    usage |= MTLTextureUsageShaderRead;
                }
                if (desc.usage & (TextureUsageRenderTarget | TextureUsageDepthStencil))
                {
                    usage |= MTLTextureUsageRenderTarget;
                }
                descriptor.usage = usage;
                bool renderable = (desc.usage & (TextureUsageRenderTarget | TextureUsageDepthStencil)) != 0;
                descriptor.storageMode = renderable || IsDepthFormat(desc.format) ? MTLStorageModePrivate : MTLStorageModeShared;
                m_texture = [device newTextureWithDescriptor:descriptor];
                if (!desc.debugName.empty())
                {
                    m_texture.label = [NSString stringWithUTF8String:desc.debugName.c_str()];
                }
            }

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
            MetalSampler(id<MTLDevice> device, const SamplerDesc& desc) : Sampler(desc)
            {
                MTLSamplerDescriptor* descriptor = [[MTLSamplerDescriptor alloc] init];
                MTLSamplerMinMagFilter filter = desc.filter == Filter::Point ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
                descriptor.minFilter = filter;
                descriptor.magFilter = filter;
                descriptor.mipFilter = desc.mipLinear ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
                descriptor.sAddressMode = ToMetal(desc.addressU);
                descriptor.tAddressMode = ToMetal(desc.addressV);
                descriptor.rAddressMode = ToMetal(desc.addressW);
                descriptor.maxAnisotropy = desc.filter == Filter::Anisotropic ? std::clamp<uint32_t>(desc.maxAnisotropy, 1, 16) : 1;
                descriptor.borderColor = desc.borderWhite ? MTLSamplerBorderColorOpaqueWhite : MTLSamplerBorderColorTransparentBlack;
                if (desc.comparison)
                {
                    descriptor.compareFunction = ToMetal(desc.compare);
                }
                descriptor.supportArgumentBuffers = NO;
                m_sampler = [device newSamplerStateWithDescriptor:descriptor];
            }

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

        class MetalSwapchain : public Swapchain
        {
        public:
            MetalSwapchain(id<MTLDevice> device, Platform::Window& window, bool vsync) : m_window(window)
            {
                NSView* view = (__bridge NSView*)window.GetNativeView();
                m_layer = [CAMetalLayer layer];
                m_layer.device = device;
                m_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
                m_layer.framebufferOnly = NO;
                m_layer.displaySyncEnabled = vsync;
                m_layer.maximumDrawableCount = 3;
                NSWindow* nsWindow = (__bridge NSWindow*)window.GetNativeHandle();
                m_layer.contentsScale = nsWindow.backingScaleFactor;
                view.wantsLayer = YES;
                view.layer = m_layer;
                Math::Vector2 size = window.GetFramebufferSize();
                Resize(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
                TextureDesc desc;
                desc.width = m_width;
                desc.height = m_height;
                desc.format = Format::BGRA8Unorm;
                desc.usage = TextureUsageRenderTarget | TextureUsageSampled;
                desc.debugName = "Swapchain";
                m_texture = std::make_unique<MetalTexture>(static_cast<id<MTLTexture>>(nil), desc);
            }

            Texture* AcquireNextTexture() override
            {
                NSWindow* nsWindow = (__bridge NSWindow*)m_window.GetNativeHandle();
                if (m_layer.contentsScale != nsWindow.backingScaleFactor)
                {
                    m_layer.contentsScale = nsWindow.backingScaleFactor;
                }
                Math::Vector2 size = m_window.GetFramebufferSize();
                if (static_cast<uint32_t>(size.x) != m_width || static_cast<uint32_t>(size.y) != m_height)
                {
                    Resize(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
                }
                if (m_width == 0 || m_height == 0)
                {
                    return nullptr;
                }
                m_drawable = [m_layer nextDrawable];
                if (!m_drawable)
                {
                    return nullptr;
                }
                m_texture->Set(m_drawable.texture);
                m_texture->SetSize(m_width, m_height);
                return m_texture.get();
            }

            void Resize(uint32_t width, uint32_t height) override
            {
                m_width = width;
                m_height = height;
                if (width > 0 && height > 0)
                {
                    m_layer.drawableSize = CGSizeMake(width, height);
                }
            }

            uint32_t GetWidth() const override { return m_width; }
            uint32_t GetHeight() const override { return m_height; }
            Format GetFormat() const override { return Format::BGRA8Unorm; }
            void SetVSync(bool enabled) override { m_layer.displaySyncEnabled = enabled; }

            id<CAMetalDrawable> TakeDrawable()
            {
                id<CAMetalDrawable> drawable = m_drawable;
                m_drawable = nil;
                m_texture->Set(nil);
                return drawable;
            }

        private:
            Platform::Window& m_window;
            CAMetalLayer* m_layer;
            id<CAMetalDrawable> m_drawable;
            std::unique_ptr<MetalTexture> m_texture;
            uint32_t m_width = 0;
            uint32_t m_height = 0;
        };

        struct TransientAllocation
        {
            id<MTLBuffer> buffer;
            size_t offset = 0;
        };

        class TransientAllocator
        {
        public:
            void Initialize(id<MTLDevice> device)
            {
                m_device = device;
            }

            void BeginFrame(uint32_t frame)
            {
                m_frame = frame % FramesInFlight;
                m_pages[m_frame].resize(std::max<size_t>(m_pages[m_frame].size(), 1));
                if (!m_pages[m_frame][0])
                {
                    m_pages[m_frame][0] = [m_device newBufferWithLength:TransientPageSize options:MTLResourceStorageModeShared];
                }
                m_page = 0;
                m_offset = 0;
            }

            TransientAllocation Allocate(const void* data, size_t size)
            {
                size_t aligned = (m_offset + TransientAlignment - 1) & ~(TransientAlignment - 1);
                if (aligned + size > TransientPageSize)
                {
                    ++m_page;
                    aligned = 0;
                    auto& pages = m_pages[m_frame];
                    if (m_page >= pages.size())
                    {
                        pages.push_back([m_device newBufferWithLength:std::max(TransientPageSize, size) options:MTLResourceStorageModeShared]);
                    }
                    else if (pages[m_page].length < size)
                    {
                        pages[m_page] = [m_device newBufferWithLength:size options:MTLResourceStorageModeShared];
                    }
                }
                id<MTLBuffer> buffer = m_pages[m_frame][m_page];
                std::memcpy(static_cast<uint8_t*>([buffer contents]) + aligned, data, size);
                m_offset = aligned + size;
                return { buffer, aligned };
            }

        private:
            id<MTLDevice> m_device;
            std::vector<id<MTLBuffer>> m_pages[FramesInFlight];
            uint32_t m_frame = 0;
            size_t m_page = 0;
            size_t m_offset = 0;
        };

        class MetalCommandList : public CommandList
        {
        public:
            void Begin(id<MTLCommandBuffer> commandBuffer, uint32_t frame, TransientAllocator* transient)
            {
                m_commandBuffer = commandBuffer;
                m_frame = frame;
                m_transient = transient;
                m_encoder = nil;
                m_pipeline = nullptr;
            }

            void End()
            {
                EndEncoding();
                m_commandBuffer = nil;
            }

            id<MTLCommandBuffer> GetCommandBuffer() const { return m_commandBuffer; }

            void BeginRenderPass(const RenderPassDesc& desc) override
            {
                EndEncoding();
                MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
                uint32_t width = 0;
                uint32_t height = 0;
                for (size_t i = 0; i < desc.colors.size(); ++i)
                {
                    const ColorAttachment& attachment = desc.colors[i];
                    auto* texture = static_cast<MetalTexture*>(attachment.texture);
                    if (!texture)
                    {
                        continue;
                    }
                    MTLRenderPassColorAttachmentDescriptor* color = pass.colorAttachments[i];
                    color.texture = texture->Get();
                    color.level = attachment.mipLevel;
                    color.slice = attachment.layer;
                    color.loadAction = attachment.load == LoadAction::Clear ? MTLLoadActionClear : (attachment.load == LoadAction::Load ? MTLLoadActionLoad : MTLLoadActionDontCare);
                    color.clearColor = MTLClearColorMake(attachment.clearColor[0], attachment.clearColor[1], attachment.clearColor[2], attachment.clearColor[3]);
                    color.storeAction = MTLStoreActionStore;
                    width = std::max<uint32_t>(texture->GetWidth() >> attachment.mipLevel, 1);
                    height = std::max<uint32_t>(texture->GetHeight() >> attachment.mipLevel, 1);
                }
                if (auto* depth = static_cast<MetalTexture*>(desc.depth.texture))
                {
                    pass.depthAttachment.texture = depth->Get();
                    pass.depthAttachment.slice = desc.depth.layer;
                    pass.depthAttachment.loadAction = desc.depth.load == LoadAction::Clear ? MTLLoadActionClear : (desc.depth.load == LoadAction::Load ? MTLLoadActionLoad : MTLLoadActionDontCare);
                    pass.depthAttachment.clearDepth = desc.depth.clearDepth;
                    pass.depthAttachment.storeAction = MTLStoreActionStore;
                    if (depth->GetFormat() == Format::Depth24Stencil8)
                    {
                        pass.stencilAttachment.texture = depth->Get();
                        pass.stencilAttachment.loadAction = MTLLoadActionClear;
                        pass.stencilAttachment.storeAction = MTLStoreActionDontCare;
                    }
                    if (width == 0)
                    {
                        width = depth->GetWidth();
                        height = depth->GetHeight();
                    }
                }
                m_encoder = [m_commandBuffer renderCommandEncoderWithDescriptor:pass];
                if (!desc.debugName.empty())
                {
                    m_encoder.label = [NSString stringWithUTF8String:desc.debugName.c_str()];
                }
                m_passWidth = width;
                m_passHeight = height;
                m_pipeline = nullptr;
                Viewport viewport;
                viewport.width = static_cast<float>(width);
                viewport.height = static_cast<float>(height);
                SetViewport(viewport);
            }

            void EndRenderPass() override
            {
                EndEncoding();
            }

            void SetPipeline(Pipeline* pipeline) override
            {
                auto* metal = static_cast<MetalPipeline*>(pipeline);
                if (!m_encoder || !metal || metal == m_pipeline)
                {
                    return;
                }
                m_pipeline = metal;
                const PipelineDesc& desc = metal->GetDesc();
                [m_encoder setRenderPipelineState:metal->GetState()];
                [m_encoder setDepthStencilState:metal->GetDepthState()];
                MTLCullMode cull = desc.rasterizer.cullMode == CullMode::None ? MTLCullModeNone : (desc.rasterizer.cullMode == CullMode::Front ? MTLCullModeFront : MTLCullModeBack);
                [m_encoder setCullMode:cull];
                [m_encoder setFrontFacingWinding:desc.rasterizer.frontCounterClockwise ? MTLWindingCounterClockwise : MTLWindingClockwise];
                [m_encoder setTriangleFillMode:desc.rasterizer.wireframe ? MTLTriangleFillModeLines : MTLTriangleFillModeFill];
                [m_encoder setDepthClipMode:desc.rasterizer.depthClip ? MTLDepthClipModeClip : MTLDepthClipModeClamp];
                [m_encoder setDepthBias:static_cast<float>(desc.rasterizer.depthBias) slopeScale:desc.rasterizer.slopeScaledDepthBias clamp:desc.rasterizer.depthBiasClamp];
                if (!desc.rasterizer.scissor)
                {
                    MTLScissorRect full = { 0, 0, m_passWidth, m_passHeight };
                    [m_encoder setScissorRect:full];
                }
            }

            void SetViewport(const Viewport& viewport) override
            {
                if (!m_encoder)
                {
                    return;
                }
                MTLViewport metal = { viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth };
                [m_encoder setViewport:metal];
            }

            void SetScissor(const ScissorRect& scissor) override
            {
                if (!m_encoder)
                {
                    return;
                }
                int32_t x = std::clamp<int32_t>(scissor.x, 0, static_cast<int32_t>(m_passWidth));
                int32_t y = std::clamp<int32_t>(scissor.y, 0, static_cast<int32_t>(m_passHeight));
                int32_t right = std::clamp<int32_t>(scissor.x + scissor.width, x, static_cast<int32_t>(m_passWidth));
                int32_t bottom = std::clamp<int32_t>(scissor.y + scissor.height, y, static_cast<int32_t>(m_passHeight));
                MTLScissorRect rect = { static_cast<NSUInteger>(x), static_cast<NSUInteger>(y), static_cast<NSUInteger>(right - x), static_cast<NSUInteger>(bottom - y) };
                [m_encoder setScissorRect:rect];
            }

            void SetVertexBuffer(uint32_t slot, Buffer* buffer, size_t offset) override
            {
                if (!m_encoder || !buffer)
                {
                    return;
                }
                [m_encoder setVertexBuffer:static_cast<MetalBuffer*>(buffer)->Get(m_frame) offset:offset atIndex:VertexBufferBase + slot];
            }

            void SetIndexBuffer(Buffer* buffer, IndexFormat format, size_t offset) override
            {
                m_indexBuffer = buffer ? static_cast<MetalBuffer*>(buffer)->Get(m_frame) : nil;
                m_indexOffset = offset;
                m_indexFormat = format;
            }

            void SetVertexData(uint32_t slot, const void* data, size_t size) override
            {
                if (!m_encoder || size == 0)
                {
                    return;
                }
                TransientAllocation allocation = m_transient->Allocate(data, size);
                [m_encoder setVertexBuffer:allocation.buffer offset:allocation.offset atIndex:VertexBufferBase + slot];
            }

            void SetIndexData(const void* data, size_t size, IndexFormat format) override
            {
                if (size == 0)
                {
                    return;
                }
                TransientAllocation allocation = m_transient->Allocate(data, size);
                m_indexBuffer = allocation.buffer;
                m_indexOffset = allocation.offset;
                m_indexFormat = format;
            }

            void SetConstants(uint32_t slot, const void* data, size_t size) override
            {
                if (!m_encoder)
                {
                    return;
                }
                TransientAllocation allocation = m_transient->Allocate(data, size);
                [m_encoder setVertexBuffer:allocation.buffer offset:allocation.offset atIndex:slot];
                [m_encoder setFragmentBuffer:allocation.buffer offset:allocation.offset atIndex:slot];
            }

            void SetConstantBuffer(uint32_t slot, Buffer* buffer) override
            {
                if (!m_encoder || !buffer)
                {
                    return;
                }
                id<MTLBuffer> metal = static_cast<MetalBuffer*>(buffer)->Get(m_frame);
                [m_encoder setVertexBuffer:metal offset:0 atIndex:slot];
                [m_encoder setFragmentBuffer:metal offset:0 atIndex:slot];
            }

            void SetTexture(uint32_t slot, Texture* texture) override
            {
                if (!m_encoder)
                {
                    return;
                }
                id<MTLTexture> metal = texture ? static_cast<MetalTexture*>(texture)->Get() : nil;
                [m_encoder setVertexTexture:metal atIndex:slot];
                [m_encoder setFragmentTexture:metal atIndex:slot];
            }

            void SetSampler(uint32_t slot, Sampler* sampler) override
            {
                if (!m_encoder || !sampler)
                {
                    return;
                }
                id<MTLSamplerState> metal = static_cast<MetalSampler*>(sampler)->Get();
                [m_encoder setVertexSamplerState:metal atIndex:slot];
                [m_encoder setFragmentSamplerState:metal atIndex:slot];
            }

            void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override
            {
                if (!m_encoder || !m_pipeline || vertexCount == 0)
                {
                    return;
                }
                [m_encoder drawPrimitives:ToMetal(m_pipeline->GetDesc().topology) vertexStart:firstVertex vertexCount:vertexCount instanceCount:instanceCount baseInstance:firstInstance];
            }

            void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) override
            {
                if (!m_encoder || !m_pipeline || !m_indexBuffer || indexCount == 0)
                {
                    return;
                }
                size_t indexSize = m_indexFormat == IndexFormat::UInt16 ? 2 : 4;
                [m_encoder drawIndexedPrimitives:ToMetal(m_pipeline->GetDesc().topology)
                                      indexCount:indexCount
                                       indexType:m_indexFormat == IndexFormat::UInt16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
                                     indexBuffer:m_indexBuffer
                               indexBufferOffset:m_indexOffset + firstIndex * indexSize
                                   instanceCount:instanceCount
                                      baseVertex:baseVertex
                                    baseInstance:firstInstance];
            }

            void CopyTexture(Texture* source, Texture* destination) override
            {
                EndEncoding();
                id<MTLBlitCommandEncoder> blit = [m_commandBuffer blitCommandEncoder];
                [blit copyFromTexture:static_cast<MetalTexture*>(source)->Get() toTexture:static_cast<MetalTexture*>(destination)->Get()];
                [blit endEncoding];
            }

            void GenerateMips(Texture* texture) override
            {
                EndEncoding();
                id<MTLBlitCommandEncoder> blit = [m_commandBuffer blitCommandEncoder];
                [blit generateMipmapsForTexture:static_cast<MetalTexture*>(texture)->Get()];
                [blit endEncoding];
            }

            void PushDebugGroup(const char* name) override
            {
                if (m_encoder)
                {
                    [m_encoder pushDebugGroup:[NSString stringWithUTF8String:name]];
                }
            }

            void PopDebugGroup() override
            {
                if (m_encoder)
                {
                    [m_encoder popDebugGroup];
                }
            }

        private:
            void EndEncoding()
            {
                if (m_encoder)
                {
                    [m_encoder endEncoding];
                    m_encoder = nil;
                }
                m_pipeline = nullptr;
            }

            id<MTLCommandBuffer> m_commandBuffer;
            id<MTLRenderCommandEncoder> m_encoder;
            MetalPipeline* m_pipeline = nullptr;
            id<MTLBuffer> m_indexBuffer;
            size_t m_indexOffset = 0;
            IndexFormat m_indexFormat = IndexFormat::UInt32;
            TransientAllocator* m_transient = nullptr;
            uint32_t m_frame = 0;
            uint32_t m_passWidth = 0;
            uint32_t m_passHeight = 0;
        };

        class MetalDevice : public Device
        {
        public:
            MetalDevice()
            {
                m_device = MTLCreateSystemDefaultDevice();
                m_queue = [m_device newCommandQueue];
                m_semaphore = dispatch_semaphore_create(FramesInFlight);
                m_transient.Initialize(m_device);
            }

            ~MetalDevice() override
            {
                WaitIdle();
            }

            bool IsValid() const { return m_device != nil; }
            Backend GetBackend() const override { return Backend::Metal; }
            std::string GetName() const override { return std::string("Metal (") + [[m_device name] UTF8String] + ")"; }

            std::unique_ptr<Buffer> CreateBuffer(const BufferDesc& desc, const void* initialData) override
            {
                return std::make_unique<MetalBuffer>(m_device, desc, initialData);
            }

            void UpdateBuffer(Buffer* buffer, const void* data, size_t size, size_t offset) override
            {
                auto* metal = static_cast<MetalBuffer*>(buffer);
                id<MTLBuffer> target = metal->Get(m_frameIndex);
                if (offset + size > target.length)
                {
                    return;
                }
                std::memcpy(static_cast<uint8_t*>([target contents]) + offset, data, size);
            }

            std::unique_ptr<Texture> CreateTexture(const TextureDesc& desc) override
            {
                auto texture = std::make_unique<MetalTexture>(m_device, desc);
                if (!texture->Get())
                {
                    Core::Log::Error("Metal: failed to create texture '" + desc.debugName + "'");
                    return nullptr;
                }
                return texture;
            }

            void UpdateTexture(Texture* texture, uint32_t mipLevel, uint32_t layer, const void* data, uint32_t rowPitch) override
            {
                auto* metal = static_cast<MetalTexture*>(texture);
                uint32_t width = std::max<uint32_t>(texture->GetWidth() >> mipLevel, 1);
                uint32_t height = std::max<uint32_t>(texture->GetHeight() >> mipLevel, 1);
                MTLRegion region = MTLRegionMake2D(0, 0, width, height);
                if (metal->Get().storageMode == MTLStorageModePrivate)
                {
                    size_t size = static_cast<size_t>(rowPitch) * height;
                    id<MTLBuffer> staging = [m_device newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];
                    id<MTLCommandBuffer> commands = [m_queue commandBuffer];
                    id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
                    [blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:rowPitch sourceBytesPerImage:size sourceSize:MTLSizeMake(width, height, 1) toTexture:metal->Get() destinationSlice:layer destinationLevel:mipLevel destinationOrigin:MTLOriginMake(0, 0, 0)];
                    [blit endEncoding];
                    [commands commit];
                    [commands waitUntilCompleted];
                    return;
                }
                [metal->Get() replaceRegion:region mipmapLevel:mipLevel slice:layer withBytes:data bytesPerRow:rowPitch bytesPerImage:static_cast<NSUInteger>(rowPitch) * height];
            }

            std::unique_ptr<Sampler> CreateSampler(const SamplerDesc& desc) override
            {
                return std::make_unique<MetalSampler>(m_device, desc);
            }

            std::unique_ptr<Shader> CreateShader(const ShaderDesc& desc, std::string& error) override
            {
                ShaderCompileResult compiled = ShaderCompiler::CompileToMetal(desc);
                if (!compiled.success)
                {
                    error = compiled.error;
                    return nullptr;
                }
                NSError* nsError = nil;
                MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
                options.mathMode = MTLMathModeFast;
                id<MTLLibrary> library = [m_device newLibraryWithSource:[NSString stringWithUTF8String:compiled.metalSource.c_str()] options:options error:&nsError];
                if (!library)
                {
                    error = std::string("MSL compile failed: ") + (nsError ? [[nsError localizedDescription] UTF8String] : "unknown") + "\n" + compiled.metalSource;
                    return nullptr;
                }
                id<MTLFunction> function = [library newFunctionWithName:[NSString stringWithUTF8String:compiled.metalEntryPoint.c_str()]];
                if (!function)
                {
                    error = "entry point '" + compiled.metalEntryPoint + "' not found";
                    return nullptr;
                }
                if (!desc.debugName.empty())
                {
                    function.label = [NSString stringWithUTF8String:desc.debugName.c_str()];
                }
                return std::make_unique<MetalShader>(desc.stage, function);
            }

            std::unique_ptr<Pipeline> CreatePipeline(const PipelineDesc& desc, std::string& error) override
            {
                MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
                descriptor.vertexFunction = desc.vertexShader ? static_cast<MetalShader*>(desc.vertexShader)->Get() : nil;
                descriptor.fragmentFunction = desc.pixelShader ? static_cast<MetalShader*>(desc.pixelShader)->Get() : nil;
                if (!desc.debugName.empty())
                {
                    descriptor.label = [NSString stringWithUTF8String:desc.debugName.c_str()];
                }

                if (!desc.vertexLayout.attributes.empty())
                {
                    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
                    for (size_t i = 0; i < desc.vertexLayout.attributes.size(); ++i)
                    {
                        const VertexAttribute& attribute = desc.vertexLayout.attributes[i];
                        vertexDescriptor.attributes[i].format = ToMetalVertex(attribute.format);
                        vertexDescriptor.attributes[i].offset = attribute.offset;
                        vertexDescriptor.attributes[i].bufferIndex = VertexBufferBase + attribute.bufferSlot;
                    }
                    for (uint32_t slot = 0; slot < MaxVertexBuffers; ++slot)
                    {
                        if (desc.vertexLayout.strides[slot] == 0)
                        {
                            continue;
                        }
                        MTLVertexBufferLayoutDescriptor* layout = vertexDescriptor.layouts[VertexBufferBase + slot];
                        layout.stride = desc.vertexLayout.strides[slot];
                        layout.stepFunction = desc.vertexLayout.perInstance[slot] ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
                        layout.stepRate = 1;
                    }
                    descriptor.vertexDescriptor = vertexDescriptor;
                }

                for (size_t i = 0; i < desc.colorFormats.size(); ++i)
                {
                    MTLRenderPipelineColorAttachmentDescriptor* color = descriptor.colorAttachments[i];
                    color.pixelFormat = ToMetal(desc.colorFormats[i]);
                    const BlendState& blend = desc.blend[i];
                    color.blendingEnabled = blend.enabled;
                    color.sourceRGBBlendFactor = ToMetal(blend.sourceColor);
                    color.destinationRGBBlendFactor = ToMetal(blend.destinationColor);
                    color.rgbBlendOperation = ToMetal(blend.colorOperation);
                    color.sourceAlphaBlendFactor = ToMetal(blend.sourceAlpha);
                    color.destinationAlphaBlendFactor = ToMetal(blend.destinationAlpha);
                    color.alphaBlendOperation = ToMetal(blend.alphaOperation);
                    MTLColorWriteMask mask = MTLColorWriteMaskNone;
                    if (blend.writeMask & ColorWriteRed)
                    {
                        mask |= MTLColorWriteMaskRed;
                    }
                    if (blend.writeMask & ColorWriteGreen)
                    {
                        mask |= MTLColorWriteMaskGreen;
                    }
                    if (blend.writeMask & ColorWriteBlue)
                    {
                        mask |= MTLColorWriteMaskBlue;
                    }
                    if (blend.writeMask & ColorWriteAlpha)
                    {
                        mask |= MTLColorWriteMaskAlpha;
                    }
                    color.writeMask = mask;
                }
                if (desc.depthFormat != Format::Unknown)
                {
                    descriptor.depthAttachmentPixelFormat = ToMetal(desc.depthFormat);
                    if (desc.depthFormat == Format::Depth24Stencil8)
                    {
                        descriptor.stencilAttachmentPixelFormat = ToMetal(desc.depthFormat);
                    }
                }
                descriptor.inputPrimitiveTopology = desc.topology == PrimitiveTopology::LineList || desc.topology == PrimitiveTopology::LineStrip ? MTLPrimitiveTopologyClassLine : (desc.topology == PrimitiveTopology::PointList ? MTLPrimitiveTopologyClassPoint : MTLPrimitiveTopologyClassTriangle);

                NSError* nsError = nil;
                id<MTLRenderPipelineState> state = [m_device newRenderPipelineStateWithDescriptor:descriptor error:&nsError];
                if (!state)
                {
                    error = std::string("pipeline '") + desc.debugName + "' failed: " + (nsError ? [[nsError localizedDescription] UTF8String] : "unknown");
                    return nullptr;
                }

                MTLDepthStencilDescriptor* depthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
                depthDescriptor.depthCompareFunction = desc.depthStencil.depthTest ? ToMetal(desc.depthStencil.depthCompare) : MTLCompareFunctionAlways;
                depthDescriptor.depthWriteEnabled = desc.depthStencil.depthWrite && desc.depthFormat != Format::Unknown;
                id<MTLDepthStencilState> depthState = [m_device newDepthStencilStateWithDescriptor:depthDescriptor];
                return std::make_unique<MetalPipeline>(desc, state, depthState);
            }

            std::unique_ptr<Swapchain> CreateSwapchain(Platform::Window& window, bool vsync) override
            {
                return std::make_unique<MetalSwapchain>(m_device, window, vsync);
            }

            CommandList& BeginFrame() override
            {
                dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);
                ++m_frameIndex;
                m_transient.BeginFrame(m_frameIndex);
                id<MTLCommandBuffer> commandBuffer = [m_queue commandBuffer];
                m_commandList.Begin(commandBuffer, m_frameIndex, &m_transient);
                m_inFrame = true;
                return m_commandList;
            }

            void EndFrame(const std::vector<Swapchain*>& present) override
            {
                id<MTLCommandBuffer> commandBuffer = m_commandList.GetCommandBuffer();
                m_commandList.End();
                for (Swapchain* swapchain : present)
                {
                    if (!swapchain)
                    {
                        continue;
                    }
                    id<CAMetalDrawable> drawable = static_cast<MetalSwapchain*>(swapchain)->TakeDrawable();
                    if (drawable)
                    {
                        [commandBuffer presentDrawable:drawable];
                    }
                }
                dispatch_semaphore_t semaphore = m_semaphore;
                [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
                    dispatch_semaphore_signal(semaphore);
                }];
                [commandBuffer commit];
                m_lastCommandBuffer = commandBuffer;
                m_inFrame = false;
            }

            bool ReadTexture(Texture* texture, std::vector<uint8_t>& pixels) override
            {
                auto* metal = static_cast<MetalTexture*>(texture);
                uint32_t bytesPerPixel = FormatBytesPerPixel(texture->GetFormat());
                if (!metal->Get() || bytesPerPixel == 0)
                {
                    return false;
                }
                WaitIdle();
                size_t rowPitch = static_cast<size_t>(texture->GetWidth()) * bytesPerPixel;
                size_t size = rowPitch * texture->GetHeight();
                id<MTLBuffer> staging = [m_device newBufferWithLength:size options:MTLResourceStorageModeShared];
                id<MTLCommandBuffer> commands = [m_queue commandBuffer];
                id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
                [blit copyFromTexture:metal->Get() sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(texture->GetWidth(), texture->GetHeight(), 1) toBuffer:staging destinationOffset:0 destinationBytesPerRow:rowPitch destinationBytesPerImage:size];
                [blit endEncoding];
                [commands commit];
                [commands waitUntilCompleted];
                pixels.resize(size);
                std::memcpy(pixels.data(), [staging contents], size);
                return true;
            }

            void WaitIdle() override
            {
                if (m_lastCommandBuffer)
                {
                    [m_lastCommandBuffer waitUntilCompleted];
                }
            }

            bool IsInFrame() const override { return m_inFrame; }

        private:
            id<MTLDevice> m_device;
            id<MTLCommandQueue> m_queue;
            id<MTLCommandBuffer> m_lastCommandBuffer;
            dispatch_semaphore_t m_semaphore;
            TransientAllocator m_transient;
            MetalCommandList m_commandList;
            uint32_t m_frameIndex = 0;
            bool m_inFrame = false;
        };
    }

    std::unique_ptr<Device> CreateMetalDevice()
    {
        auto device = std::make_unique<MetalDevice>();
        if (!device->IsValid())
        {
            Core::Log::Error("Metal is not available on this machine");
            return nullptr;
        }
        return device;
    }
}
