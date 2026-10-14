#pragma once

#include "RHI/Metal/MetalResources.h"
#include "RHI/Resources.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <memory>

namespace BrightBox3D::Platform
{
    class Window;
}

namespace BrightBox3D::RHI
{
    class MetalSwapchain : public Swapchain
    {
    public:
        MetalSwapchain(id<MTLDevice> device, Platform::Window& window, bool vsync);

        Texture* AcquireNextTexture() override;
        void Resize(uint32_t width, uint32_t height) override;
        uint32_t GetWidth() const override { return m_width; }
        uint32_t GetHeight() const override { return m_height; }
        Format GetFormat() const override { return Format::BGRA8Unorm; }
        void SetVSync(bool enabled) override { m_layer.displaySyncEnabled = enabled; }

        id<CAMetalDrawable> TakeDrawable();

    private:
        void SyncContentsScale();

        Platform::Window& m_window;
        CAMetalLayer* m_layer;
        id<CAMetalDrawable> m_drawable;
        std::unique_ptr<MetalTexture> m_texture;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
    };
}
