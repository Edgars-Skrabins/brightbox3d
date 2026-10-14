#pragma once

#include "RHI/DX11/DX11Resources.h"
#include "RHI/Resources.h"

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <memory>

namespace BrightBox3D::Platform
{
    class Window;
}

namespace BrightBox3D::RHI
{
    class DX11Swapchain : public Swapchain
    {
    public:
        DX11Swapchain(ID3D11Device* device, IDXGIFactory2* factory, Platform::Window& window, bool vsync);

        Texture* AcquireNextTexture() override;
        void Resize(uint32_t width, uint32_t height) override;
        uint32_t GetWidth() const override { return m_width; }
        uint32_t GetHeight() const override { return m_height; }
        Format GetFormat() const override { return Format::BGRA8Unorm; }
        void SetVSync(bool enabled) override { m_vsync = enabled; }

        void Present();

    private:
        void AcquireBackbuffer();

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
}
