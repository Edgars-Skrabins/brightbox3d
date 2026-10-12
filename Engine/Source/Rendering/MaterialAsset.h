#pragma once

#include "Assets/AssetDatabase.h"
#include "Math/Color.h"
#include "Math/Vector2.h"
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

        BrightProperty(EditAnywhere, Category = "Surface")
        ShadingModel m_shadingModel = ShadingModel::Lit;

        BrightProperty(EditAnywhere, Category = "Surface")
        BlendMode m_blendMode = BlendMode::Opaque;

        BrightProperty(EditAnywhere, Category = "Surface")
        float m_alphaCutoff = 0.5f;

        BrightProperty(EditAnywhere, Category = "Surface")
        bool m_doubleSided = false;

        BrightProperty(EditAnywhere, Category = "Base Color")
        Math::Color m_baseColor = Math::Color::White();

        BrightProperty(EditAnywhere, Category = "Base Color")
        Assets::AssetReference<TextureAsset> m_baseColorMap;

        BrightProperty(EditAnywhere, Category = "Surface")
        float m_metallic = 0.0f;

        BrightProperty(EditAnywhere, Category = "Surface")
        float m_roughness = 0.5f;

        BrightProperty(EditAnywhere, Category = "Surface")
        Assets::AssetReference<TextureAsset> m_metallicRoughnessMap;

        BrightProperty(EditAnywhere, Category = "Normal")
        Assets::AssetReference<TextureAsset> m_normalMap;

        BrightProperty(EditAnywhere, Category = "Normal")
        float m_normalStrength = 1.0f;

        BrightProperty(EditAnywhere, Category = "Occlusion")
        Assets::AssetReference<TextureAsset> m_occlusionMap;

        BrightProperty(EditAnywhere, Category = "Emission")
        Math::Color m_emissiveColor = Math::Color::Black();

        BrightProperty(EditAnywhere, Category = "Emission")
        float m_emissiveIntensity = 1.0f;

        BrightProperty(EditAnywhere, Category = "Emission")
        Assets::AssetReference<TextureAsset> m_emissiveMap;

        BrightProperty(EditAnywhere, Category = "Mapping")
        Math::Vector2 m_tiling{ 1.0f, 1.0f };

        BrightProperty(EditAnywhere, Category = "Mapping")
        Math::Vector2 m_offset;
    };
}
