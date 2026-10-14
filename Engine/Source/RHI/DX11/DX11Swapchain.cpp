#include "RHI/DX11/DX11Swapchain.h"

#include "Platform/Window.h"

#include <algorithm>

namespace BrightBox3D::RHI
{
    DX11Swapchain::DX11Swapchain(ID3D11Device* device, IDXGIFactory2* factory, Platform::Window& window, bool vsync) : m_device(device), m_window(window), m_vsync(vsync)
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

    Texture* DX11Swapchain::AcquireNextTexture()
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

    void DX11Swapchain::Resize(uint32_t width, uint32_t height)
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

    void DX11Swapchain::Present()
    {
        if (!m_acquired || !m_swapchain)
        {
            return;
        }
        m_acquired = false;
        UINT flags = !m_vsync && m_allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
        m_swapchain->Present(m_vsync ? 1 : 0, flags);
    }

    void DX11Swapchain::AcquireBackbuffer()
    {
        if (m_swapchain)
        {
            m_swapchain->GetBuffer(0, IID_PPV_ARGS(&m_texture->texture));
        }
    }
}
