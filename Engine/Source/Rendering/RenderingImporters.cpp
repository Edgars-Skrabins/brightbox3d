#include "Rendering/RenderingImporters.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "Math/Color.h"
#include "Reflection/PropertyTraits.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/TextureAsset.h"
#include "Serialization/ObjectSerializer.h"

#include <stb_image.h>

#include <cstring>
#include <unordered_map>

namespace BrightBox3D::Rendering
{
    namespace
    {
        float SrgbToLinear(float value)
        {
            return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        float LinearToSrgb(float value)
        {
            return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
        }

        std::vector<TextureMip> BuildMips8(const uint8_t* pixels, uint32_t width, uint32_t height, bool srgb, bool normalMap, bool generate)
        {
            std::vector<TextureMip> mips;
            TextureMip base;
            base.width = width;
            base.height = height;
            base.pixels.assign(pixels, pixels + static_cast<size_t>(width) * height * 4);
            mips.push_back(std::move(base));
            if (!generate)
            {
                return mips;
            }
            float lookup[256];
            for (int i = 0; i < 256; ++i)
            {
                lookup[i] = srgb ? SrgbToLinear(i / 255.0f) : i / 255.0f;
            }
            while (mips.back().width > 1 || mips.back().height > 1)
            {
                const TextureMip& previous = mips.back();
                TextureMip next;
                next.width = std::max<uint32_t>(previous.width / 2, 1);
                next.height = std::max<uint32_t>(previous.height / 2, 1);
                next.pixels.resize(static_cast<size_t>(next.width) * next.height * 4);
                for (uint32_t y = 0; y < next.height; ++y)
                {
                    for (uint32_t x = 0; x < next.width; ++x)
                    {
                        float sum[4] = { 0, 0, 0, 0 };
                        int samples = 0;
                        for (uint32_t dy = 0; dy < 2; ++dy)
                        {
                            for (uint32_t dx = 0; dx < 2; ++dx)
                            {
                                uint32_t sx = std::min(x * 2 + dx, previous.width - 1);
                                uint32_t sy = std::min(y * 2 + dy, previous.height - 1);
                                const uint8_t* texel = &previous.pixels[(static_cast<size_t>(sy) * previous.width + sx) * 4];
                                for (int c = 0; c < 3; ++c)
                                {
                                    sum[c] += normalMap ? texel[c] / 255.0f * 2.0f - 1.0f : lookup[texel[c]];
                                }
                                sum[3] += texel[3] / 255.0f;
                                ++samples;
                            }
                        }
                        uint8_t* out = &next.pixels[(static_cast<size_t>(y) * next.width + x) * 4];
                        if (normalMap)
                        {
                            Math::Vector3 normal = Math::Vector3(sum[0], sum[1], sum[2]).Normalized();
                            out[0] = static_cast<uint8_t>(std::clamp((normal.x * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
                            out[1] = static_cast<uint8_t>(std::clamp((normal.y * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
                            out[2] = static_cast<uint8_t>(std::clamp((normal.z * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
                        }
                        else
                        {
                            for (int c = 0; c < 3; ++c)
                            {
                                float value = sum[c] / static_cast<float>(samples);
                                value = srgb ? LinearToSrgb(value) : value;
                                out[c] = static_cast<uint8_t>(std::clamp(value * 255.0f + 0.5f, 0.0f, 255.0f));
                            }
                        }
                        out[3] = static_cast<uint8_t>(std::clamp(sum[3] / static_cast<float>(samples) * 255.0f + 0.5f, 0.0f, 255.0f));
                    }
                }
                mips.push_back(std::move(next));
            }
            return mips;
        }

        std::vector<TextureMip> BuildMipsFloat(const float* pixels, uint32_t width, uint32_t height, bool generate)
        {
            std::vector<TextureMip> mips;
            TextureMip base;
            base.width = width;
            base.height = height;
            base.pixels.resize(static_cast<size_t>(width) * height * 16);
            std::memcpy(base.pixels.data(), pixels, base.pixels.size());
            mips.push_back(std::move(base));
            while (generate && (mips.back().width > 1 || mips.back().height > 1))
            {
                const TextureMip& previous = mips.back();
                const float* source = reinterpret_cast<const float*>(previous.pixels.data());
                TextureMip next;
                next.width = std::max<uint32_t>(previous.width / 2, 1);
                next.height = std::max<uint32_t>(previous.height / 2, 1);
                next.pixels.resize(static_cast<size_t>(next.width) * next.height * 16);
                float* target = reinterpret_cast<float*>(next.pixels.data());
                for (uint32_t y = 0; y < next.height; ++y)
                {
                    for (uint32_t x = 0; x < next.width; ++x)
                    {
                        for (int c = 0; c < 4; ++c)
                        {
                            float sum = 0.0f;
                            for (uint32_t dy = 0; dy < 2; ++dy)
                            {
                                for (uint32_t dx = 0; dx < 2; ++dx)
                                {
                                    uint32_t sx = std::min(x * 2 + dx, previous.width - 1);
                                    uint32_t sy = std::min(y * 2 + dy, previous.height - 1);
                                    sum += source[(static_cast<size_t>(sy) * previous.width + sx) * 4 + c];
                                }
                            }
                            target[(static_cast<size_t>(y) * next.width + x) * 4 + c] = sum * 0.25f;
                        }
                    }
                }
                mips.push_back(std::move(next));
            }
            return mips;
        }

        template <typename E>
        E ReadEnumSetting(const Serialization::JsonValue& settings, const char* name, E fallback)
        {
            std::string text = Serialization::GetString(settings, name);
            if (const Reflection::EnumEntry* entry = Reflection::StaticEnum<E>().FindByName(text))
            {
                return static_cast<E>(entry->value);
            }
            return fallback;
        }
    }

    void TextureImporter::WriteDefaultSettings(Serialization::JsonValue& settings, Serialization::JsonAllocator& allocator) const
    {
        settings.AddMember("srgb", true, allocator);
        settings.AddMember("normalMap", false, allocator);
        settings.AddMember("generateMips", true, allocator);
        settings.AddMember("filter", "Trilinear", allocator);
        settings.AddMember("wrap", "Repeat", allocator);
    }

    bool TextureImporter::Cook(const Assets::AssetRecord& record, const std::filesystem::path& source, const Serialization::JsonValue& settings, std::vector<uint8_t>& output, std::string& error)
    {
        bool normalMap = Serialization::GetBool(settings, "normalMap", false);
        bool srgb = Serialization::GetBool(settings, "srgb", true) && !normalMap;
        bool generateMips = Serialization::GetBool(settings, "generateMips", true);
        TextureFilterMode filter = ReadEnumSetting(settings, "filter", TextureFilterMode::Trilinear);
        TextureWrapMode wrap = ReadEnumSetting(settings, "wrap", TextureWrapMode::Repeat);

        std::string path = source.string();
        int width = 0;
        int height = 0;
        int channels = 0;
        TextureAsset texture;
        if (stbi_is_hdr(path.c_str()))
        {
            float* pixels = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
            if (!pixels)
            {
                error = stbi_failure_reason();
                return false;
            }
            texture.SetData(TexturePixelFormat::RGBA32Float, BuildMipsFloat(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), generateMips), filter, wrap, false);
            stbi_image_free(pixels);
        }
        else
        {
            stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
            if (!pixels)
            {
                error = stbi_failure_reason();
                return false;
            }
            texture.SetData(srgb ? TexturePixelFormat::RGBA8Srgb : TexturePixelFormat::RGBA8, BuildMips8(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), srgb, normalMap, generateMips), filter, wrap, normalMap);
            stbi_image_free(pixels);
        }
        output = texture.Serialize();
        return true;
    }

    std::shared_ptr<Assets::Asset> TextureImporter::Load(const Assets::AssetRecord& record, const uint8_t* data, size_t size, std::string& error)
    {
        auto texture = std::make_shared<TextureAsset>();
        if (!texture->Deserialize(data, size))
        {
            error = "corrupt cooked texture";
            return nullptr;
        }
        return texture;
    }

    std::shared_ptr<Assets::Asset> MaterialImporter::CreateAsset(Serialization::JsonDocument&& document, std::string& error)
    {
        auto material = std::make_shared<MaterialAsset>();
        Serialization::DeserializeProperties(*material, document);
        return material;
    }

    void RegisterRenderingAssets()
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        database.RegisterImporter(Assets::AssetType::Texture, std::make_shared<TextureImporter>());
        database.RegisterImporter(Assets::AssetType::Material, std::make_shared<MaterialImporter>());
        BuiltinAssets::Register();
    }

    namespace
    {
        struct Registration
        {
            Registration()
            {
                RegisterRenderingAssets();
            }
        };

        Registration s_registration;
    }
}
