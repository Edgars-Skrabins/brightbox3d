#pragma once

#include "Assets/AssetImporter.h"

namespace BrightBox3D::Rendering
{
    class TextureImporter : public Assets::AssetImporter
    {
    public:
        uint32_t GetVersion() const override { return 2; }
        Core::Result<std::vector<uint8_t>> Cook(const Assets::AssetRecord& record, const std::filesystem::path& source, const Reflection::EngineObject* settings) override;
        Core::Result<std::shared_ptr<Assets::Asset>> Load(const Assets::AssetRecord& record, const uint8_t* data, size_t size) override;
        const Reflection::TypeInfo* GetSettingsType() const override;
    };
}
