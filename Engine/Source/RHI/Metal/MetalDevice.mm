#include "RHI/Device.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "RHI/Metal/MetalCommandList.h"
#include "RHI/Metal/MetalConversions.h"
#include "RHI/Metal/MetalResources.h"
#include "RHI/Metal/MetalSwapchain.h"
#include "RHI/Metal/MetalTransientAllocator.h"
#include "RHI/ShaderCompiler.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cstring>

namespace BrightBox3D::RHI
{
    namespace
    {
        constexpr NSUInteger VertexStepRate = 1;

        NSString* ToNSString(const std::string& text)
        {
            return [NSString stringWithUTF8String:text.c_str()];
        }

        std::string ErrorText(NSError* error)
        {
            return error ? [[error localizedDescription] UTF8String] : Core::Text(Core::EngineText::UnknownError);
        }

        MTLVertexDescriptor* BuildVertexDescriptor(const VertexLayout& layout)
        {
            MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
            for (size_t i = 0; i < layout.attributes.size(); ++i)
            {
                const VertexAttribute& attribute = layout.attributes[i];
                vertexDescriptor.attributes[i].format = ToMetalVertex(attribute.format);
                vertexDescriptor.attributes[i].offset = attribute.offset;
                vertexDescriptor.attributes[i].bufferIndex = VertexBufferBase + attribute.bufferSlot;
            }
            for (uint32_t slot = 0; slot < MaxVertexBuffers; ++slot)
            {
                if (layout.strides[slot] == 0)
                {
                    continue;
                }
                MTLVertexBufferLayoutDescriptor* bufferLayout = vertexDescriptor.layouts[VertexBufferBase + slot];
                bufferLayout.stride = layout.strides[slot];
                bufferLayout.stepFunction = layout.perInstance[slot] ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
                bufferLayout.stepRate = VertexStepRate;
            }
            return vertexDescriptor;
        }

        void ConfigureColorAttachment(MTLRenderPipelineColorAttachmentDescriptor* color, Format format, const BlendState& blend)
        {
            color.pixelFormat = ToMetal(format);
            color.blendingEnabled = blend.enabled;
            color.sourceRGBBlendFactor = ToMetal(blend.sourceColor);
            color.destinationRGBBlendFactor = ToMetal(blend.destinationColor);
            color.rgbBlendOperation = ToMetal(blend.colorOperation);
            color.sourceAlphaBlendFactor = ToMetal(blend.sourceAlpha);
            color.destinationAlphaBlendFactor = ToMetal(blend.destinationAlpha);
            color.alphaBlendOperation = ToMetal(blend.alphaOperation);
            color.writeMask = ToMetalWriteMask(blend.writeMask);
        }

        MTLRenderPipelineDescriptor* BuildPipelineDescriptor(const PipelineDesc& desc)
        {
            MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
            descriptor.vertexFunction = desc.vertexShader ? static_cast<MetalShader*>(desc.vertexShader)->Get() : nil;
            descriptor.fragmentFunction = desc.pixelShader ? static_cast<MetalShader*>(desc.pixelShader)->Get() : nil;
            if (!desc.debugName.empty())
            {
                descriptor.label = ToNSString(desc.debugName);
            }
            if (!desc.vertexLayout.attributes.empty())
            {
                descriptor.vertexDescriptor = BuildVertexDescriptor(desc.vertexLayout);
            }
            for (size_t i = 0; i < desc.colorFormats.size(); ++i)
            {
                ConfigureColorAttachment(descriptor.colorAttachments[i], desc.colorFormats[i], desc.blend[i]);
            }
            if (desc.depthFormat != Format::Unknown)
            {
                descriptor.depthAttachmentPixelFormat = ToMetal(desc.depthFormat);
                if (desc.depthFormat == Format::Depth24Stencil8)
                {
                    descriptor.stencilAttachmentPixelFormat = ToMetal(desc.depthFormat);
                }
            }
            descriptor.inputPrimitiveTopology = ToMetalTopologyClass(desc.topology);
            return descriptor;
        }

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
                id<MTLBuffer> target = static_cast<MetalBuffer*>(buffer)->Get(m_frameIndex);
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
                    Core::Log::Error(Core::Text(Core::EngineText::MetalTextureFailed, desc.debugName));
                    return nullptr;
                }
                return texture;
            }

            void UpdateTexture(Texture* texture, uint32_t mipLevel, uint32_t layer, const void* data, uint32_t rowPitch) override
            {
                auto* metal = static_cast<MetalTexture*>(texture);
                uint32_t width = std::max<uint32_t>(texture->GetWidth() >> mipLevel, 1);
                uint32_t height = std::max<uint32_t>(texture->GetHeight() >> mipLevel, 1);
                if (metal->Get().storageMode == MTLStorageModePrivate)
                {
                    UploadThroughStaging(metal->Get(), mipLevel, layer, data, rowPitch, width, height);
                    return;
                }
                [metal->Get() replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:mipLevel slice:layer withBytes:data bytesPerRow:rowPitch bytesPerImage:static_cast<NSUInteger>(rowPitch) * height];
            }

            std::unique_ptr<Sampler> CreateSampler(const SamplerDesc& desc) override
            {
                return std::make_unique<MetalSampler>(m_device, desc);
            }

            Core::Result<std::unique_ptr<Shader>> CreateShader(const ShaderDesc& desc) override
            {
                Core::Result<MetalShaderSource> compiled = ShaderCompiler::CompileToMetal(desc);
                if (!compiled)
                {
                    return compiled.GetError();
                }
                NSError* nsError = nil;
                MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
                options.mathMode = MTLMathModeFast;
                id<MTLLibrary> library = [m_device newLibraryWithSource:ToNSString(compiled->source) options:options error:&nsError];
                if (!library)
                {
                    return Core::Error(Core::EngineText::MslCompileFailed, ErrorText(nsError), compiled->source);
                }
                id<MTLFunction> function = [library newFunctionWithName:ToNSString(compiled->entryPoint)];
                if (!function)
                {
                    return Core::Error(Core::EngineText::EntryPointMissing, compiled->entryPoint);
                }
                if (!desc.debugName.empty())
                {
                    function.label = ToNSString(desc.debugName);
                }
                return std::make_unique<MetalShader>(desc.stage, function);
            }

            Core::Result<std::unique_ptr<Pipeline>> CreatePipeline(const PipelineDesc& desc) override
            {
                NSError* nsError = nil;
                id<MTLRenderPipelineState> state = [m_device newRenderPipelineStateWithDescriptor:BuildPipelineDescriptor(desc) error:&nsError];
                if (!state)
                {
                    return Core::Error(Core::EngineText::PipelineCreateFailed, desc.debugName, ErrorText(nsError));
                }
                return std::make_unique<MetalPipeline>(desc, state, CreateDepthState(desc));
            }

            std::unique_ptr<Swapchain> CreateSwapchain(Platform::Window& window, PresentMode presentMode) override
            {
                return std::make_unique<MetalSwapchain>(m_device, window, presentMode);
            }

            CommandList& BeginFrame() override
            {
                dispatch_semaphore_wait(m_semaphore, DISPATCH_TIME_FOREVER);
                ++m_frameIndex;
                m_transient.BeginFrame(m_frameIndex);
                m_commandList.Begin([m_queue commandBuffer], m_frameIndex, &m_transient);
                m_inFrame = true;
                return m_commandList;
            }

            void EndFrame(const std::vector<Swapchain*>& present) override
            {
                id<MTLCommandBuffer> commandBuffer = m_commandList.GetCommandBuffer();
                m_commandList.End();
                PresentDrawables(commandBuffer, present);
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
            void UploadThroughStaging(id<MTLTexture> texture, uint32_t mipLevel, uint32_t layer, const void* data, uint32_t rowPitch, uint32_t width, uint32_t height)
            {
                size_t size = static_cast<size_t>(rowPitch) * height;
                id<MTLBuffer> staging = [m_device newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];
                id<MTLCommandBuffer> commands = [m_queue commandBuffer];
                id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
                [blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:rowPitch sourceBytesPerImage:size sourceSize:MTLSizeMake(width, height, 1) toTexture:texture destinationSlice:layer destinationLevel:mipLevel destinationOrigin:MTLOriginMake(0, 0, 0)];
                [blit endEncoding];
                [commands commit];
                [commands waitUntilCompleted];
            }

            id<MTLDepthStencilState> CreateDepthState(const PipelineDesc& desc) const
            {
                MTLDepthStencilDescriptor* depthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
                depthDescriptor.depthCompareFunction = desc.depthStencil.depthTest ? ToMetal(desc.depthStencil.depthCompare) : MTLCompareFunctionAlways;
                depthDescriptor.depthWriteEnabled = desc.depthStencil.depthWrite && desc.depthFormat != Format::Unknown;
                return [m_device newDepthStencilStateWithDescriptor:depthDescriptor];
            }

            void PresentDrawables(id<MTLCommandBuffer> commandBuffer, const std::vector<Swapchain*>& present)
            {
                for (Swapchain* swapchain : present)
                {
                    if (!swapchain)
                    {
                        continue;
                    }
                    if (id<CAMetalDrawable> drawable = static_cast<MetalSwapchain*>(swapchain)->TakeDrawable())
                    {
                        [commandBuffer presentDrawable:drawable];
                    }
                }
            }

            id<MTLDevice> m_device;
            id<MTLCommandQueue> m_queue;
            id<MTLCommandBuffer> m_lastCommandBuffer;
            dispatch_semaphore_t m_semaphore;
            MetalTransientAllocator m_transient;
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
            Core::Log::Error(Core::Text(Core::EngineText::MetalUnavailable));
            return nullptr;
        }
        return device;
    }
}
