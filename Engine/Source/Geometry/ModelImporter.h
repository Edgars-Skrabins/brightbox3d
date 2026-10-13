#pragma once

#include "Assets/AssetImporter.h"

namespace BrightBox3D::Geometry
{
    class ModelImporter : public Assets::AssetImporter
    {
    public:
        uint32_t GetVersion() const override { return 2; }
        bool Cook(const Assets::AssetRecord& record, const std::filesystem::path& source, const Serialization::JsonValue& settings, std::vector<uint8_t>& output, std::string& error) override;
        std::shared_ptr<Assets::Asset> Load(const Assets::AssetRecord& record, const uint8_t* data, size_t size, std::string& error) override;
        void WriteDefaultSettings(Serialization::JsonValue& settings, Serialization::JsonAllocator& allocator) const override;
    };

    void RegisterGeometryAssets();
}
