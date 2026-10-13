#pragma once

#include "Assets/AssetDatabase.h"
#include "Math/Color.h"
#include "Math/Vector2.h"
#include "Rendering/Categories.h"
#include "Rendering/TextureAsset.h"

namespace BrightBox3D::Rendering
{
    BrightEnum()
    enum class ShadingModel
    {
        Lit,
        Unlit
    };

    BrightEnum()
    enum class BlendMode
    {
        Opaque,
        Masked,
        Transparent
    };

    BrightClass()
    class MaterialAsset : public Assets::Asset
    {
        BrightBody()
    public:
        static constexpr const char* AssetTypeName = "Material";

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Surface)
        ShadingModel m_shadingModel = ShadingModel::Lit;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Surface)
        BlendMode m_blendMode = BlendMode::Opaque;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Surface)
        float m_alphaCutoff = 0.5f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Surface)
        bool m_doubleSided = false;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::BaseColor)
        Math::Color m_baseColor = Math::Color::White();

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::BaseColor)
        Assets::AssetReference<TextureAsset> m_baseColorMap;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Surface)
        float m_metallic = 0.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Surface)
        float m_roughness = 0.5f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Surface)
        Assets::AssetReference<TextureAsset> m_metallicRoughnessMap;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Normal)
        Assets::AssetReference<TextureAsset> m_normalMap;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Normal)
        float m_normalStrength = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Occlusion)
        Assets::AssetReference<TextureAsset> m_occlusionMap;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Emission)
        Math::Color m_emissiveColor = Math::Color::Black();

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Emission)
        float m_emissiveIntensity = 1.0f;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Emission)
        Assets::AssetReference<TextureAsset> m_emissiveMap;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Mapping)
        Math::Vector2 m_tiling{ 1.0f, 1.0f };

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Mapping)
        Math::Vector2 m_offset;
    };
}
