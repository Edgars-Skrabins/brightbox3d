#include "RHI/Metal/MetalSwapchain.h"

#include "Platform/Window.h"

#import <Cocoa/Cocoa.h>

namespace BrightBox3D::RHI
{
    namespace
    {
        constexpr NSUInteger MaxDrawableCount = 3;
        constexpr Format SwapchainFormat = Format::BGRA8Unorm;
        constexpr const char* SwapchainDebugName = "Swapchain";
    }

    MetalSwapchain::MetalSwapchain(id<MTLDevice> device, Platform::Window& window, bool vsync) : m_window(window)
    {
        NSView* view = (__bridge NSView*)window.GetNativeView();
        m_layer = [CAMetalLayer layer];
        m_layer.device = device;
        m_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        m_layer.framebufferOnly = NO;
        m_layer.displaySyncEnabled = vsync;
        m_layer.maximumDrawableCount = MaxDrawableCount;
        NSWindow* nsWindow = (__bridge NSWindow*)window.GetNativeHandle();
        m_layer.contentsScale = nsWindow.backingScaleFactor;
        view.wantsLayer = YES;
        view.layer = m_layer;
        Math::Vector2 size = window.GetFramebufferSize();
        Resize(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
        TextureDesc desc;
        desc.width = m_width;
        desc.height = m_height;
        desc.format = SwapchainFormat;
        desc.usage = TextureUsageRenderTarget | TextureUsageSampled;
        desc.debugName = SwapchainDebugName;
        m_texture = std::make_unique<MetalTexture>(static_cast<id<MTLTexture>>(nil), desc);
    }

    void MetalSwapchain::SyncContentsScale()
    {
        NSWindow* nsWindow = (__bridge NSWindow*)m_window.GetNativeHandle();
        if (m_layer.contentsScale != nsWindow.backingScaleFactor)
        {
            m_layer.contentsScale = nsWindow.backingScaleFactor;
        }
    }

    Texture* MetalSwapchain::AcquireNextTexture()
    {
        SyncContentsScale();
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

    void MetalSwapchain::Resize(uint32_t width, uint32_t height)
    {
        m_width = width;
        m_height = height;
        if (width > 0 && height > 0)
        {
            m_layer.drawableSize = CGSizeMake(width, height);
        }
    }

    id<CAMetalDrawable> MetalSwapchain::TakeDrawable()
    {
        id<CAMetalDrawable> drawable = m_drawable;
        m_drawable = nil;
        m_texture->Set(nil);
        return drawable;
    }
}
