#include "Rendering/TextureImporter.h"

#include "Core/EngineText.h"
#include "Rendering/MipGenerator.h"
#include "Rendering/TextureAsset.h"
#include "Rendering/TextureImportSettings.h"

#include <stb_image.h>

namespace BrightBox3D::Rendering
{
    namespace
    {
        MipGenerator::MipChain MipChainFor(const TextureImportSettings& settings)
        {
            return settings.generateMips ? MipGenerator::MipChain::Full : MipGenerator::MipChain::BaseOnly;
        }

        constexpr int RequestedChannels = 4;

        Core::Result<void> LoadHdr(const std::string& path, const TextureImportSettings& settings, TextureAsset& texture)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            float* pixels = stbi_loadf(path.c_str(), &width, &height, &channels, RequestedChannels);
            if (!pixels)
            {
                return Core::Error(Core::EngineText::StbImageError, std::string(stbi_failure_reason()));
            }
            texture.SetData(TexturePixelFormat::RGBA32Float, MipGenerator::BuildMipsFloat(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), MipChainFor(settings)), settings.filter, settings.wrap, false);
            stbi_image_free(pixels);
            return {};
        }

        Core::Result<void> LoadLdr(const std::string& path, const TextureImportSettings& settings, TextureAsset& texture)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, RequestedChannels);
            if (!pixels)
            {
                return Core::Error(Core::EngineText::StbImageError, std::string(stbi_failure_reason()));
            }
            bool normalMap = settings.normalMap;
            bool srgb = settings.srgb && !normalMap;
            texture.SetData(srgb ? TexturePixelFormat::RGBA8Srgb : TexturePixelFormat::RGBA8, MipGenerator::BuildMips8(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), normalMap ? MipGenerator::MipEncoding::NormalMap : (srgb ? MipGenerator::MipEncoding::Srgb : MipGenerator::MipEncoding::Linear), MipChainFor(settings)), settings.filter, settings.wrap, normalMap);
            stbi_image_free(pixels);
            return {};
        }
    }

    const Reflection::TypeInfo* TextureImporter::GetSettingsType() const
    {
        return &TextureImportSettings::StaticType();
    }

    Core::Result<std::vector<uint8_t>> TextureImporter::Cook(const Assets::AssetRecord& record, const std::filesystem::path& source, const Reflection::EngineObject* settings)
    {
        const auto& importSettings = static_cast<const TextureImportSettings&>(*settings);
        std::string path = source.string();
        TextureAsset texture;
        Core::Result<void> loaded = stbi_is_hdr(path.c_str()) ? LoadHdr(path, importSettings, texture) : LoadLdr(path, importSettings, texture);
        if (!loaded)
        {
            return loaded.GetError();
        }
        return texture.Serialize();
    }

    Core::Result<std::shared_ptr<Assets::Asset>> TextureImporter::Load(const Assets::AssetRecord& record, const uint8_t* data, size_t size)
    {
        auto texture = std::make_shared<TextureAsset>();
        if (!texture->Deserialize(data, size))
        {
            return Core::Error(Core::EngineText::CorruptCookedTexture);
        }
        return texture;
    }
}
