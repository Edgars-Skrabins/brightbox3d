#pragma once

#include "Assets/TextAsset.h"
#include "UI/FontData.h"

namespace BrightBox3D::UI
{
    BrightClass()
    class ScreenAsset : public Assets::TextAsset
    {
        BrightBody()
    public:
        static constexpr Assets::AssetType Type = Assets::AssetType::Screen;
    };

    BrightClass()
    class WidgetAsset : public Assets::TextAsset
    {
        BrightBody()
    public:
        static constexpr Assets::AssetType Type = Assets::AssetType::Widget;
    };

    BrightClass()
    class StyleSheetAsset : public Assets::TextAsset
    {
        BrightBody()
    public:
        static constexpr Assets::AssetType Type = Assets::AssetType::StyleSheet;
    };

    BrightClass()
    class FontAsset : public Assets::Asset
    {
        BrightBody()
    public:
        static constexpr Assets::AssetType Type = Assets::AssetType::Font;

        FontData& GetData() { return m_data; }
        const FontData& GetData() const { return m_data; }

    private:
        FontData m_data;
    };

    void RegisterUIAssets();
}
