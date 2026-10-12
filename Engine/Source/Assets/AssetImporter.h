#pragma once

#include "Assets/Asset.h"
#include "Serialization/Json.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Assets
{
    struct AssetRecord;

    class AssetImporter
    {
    public:
        virtual ~AssetImporter() = default;

        virtual uint32_t GetVersion() const = 0;
        virtual bool Cook(const AssetRecord& record, const std::filesystem::path& source, const Serialization::JsonValue& settings, std::vector<uint8_t>& output, std::string& error) = 0;
        virtual std::shared_ptr<Asset> Load(const AssetRecord& record, const uint8_t* data, size_t size, std::string& error) = 0;
        virtual void WriteDefaultSettings(Serialization::JsonValue& settings, Serialization::JsonAllocator& allocator) const {}
    };

    class JsonAssetImporter : public AssetImporter
    {
    public:
        uint32_t GetVersion() const override { return 1; }
        bool Cook(const AssetRecord& record, const std::filesystem::path& source, const Serialization::JsonValue& settings, std::vector<uint8_t>& output, std::string& error) override;
        std::shared_ptr<Asset> Load(const AssetRecord& record, const uint8_t* data, size_t size, std::string& error) override;

    protected:
        virtual std::shared_ptr<Asset> CreateAsset(Serialization::JsonDocument&& document, std::string& error);
    };

    class TextAssetImporter : public AssetImporter
    {
    public:
        uint32_t GetVersion() const override { return 1; }
        bool Cook(const AssetRecord& record, const std::filesystem::path& source, const Serialization::JsonValue& settings, std::vector<uint8_t>& output, std::string& error) override;
        std::shared_ptr<Asset> Load(const AssetRecord& record, const uint8_t* data, size_t size, std::string& error) override;
    };
}
