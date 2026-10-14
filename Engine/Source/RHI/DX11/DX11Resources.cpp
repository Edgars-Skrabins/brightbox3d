#include "RHI/DX11/DX11Resources.h"

#include "RHI/DX11/DX11Conversions.h"

namespace BrightBox3D::RHI
{
    ID3D11RenderTargetView* DX11Texture::GetRenderView(ID3D11Device* device, uint32_t mip, uint32_t layer)
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

    ID3D11DepthStencilView* DX11Texture::GetDepthView(ID3D11Device* device, uint32_t layer)
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
}
