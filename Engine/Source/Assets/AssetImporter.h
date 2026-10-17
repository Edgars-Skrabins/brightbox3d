#pragma once

#include "Assets/Asset.h"
#include "Core/Result.h"
#include "Serialization/Json.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Reflection
{
    class EngineObject;
    class TypeInfo;
}

namespace BrightBox3D::Assets
{
    struct AssetRecord;

    class AssetImporter
    {
    public:
        virtual ~AssetImporter() = default;

        virtual uint32_t GetVersion() const = 0;
        virtual Core::Result<std::vector<uint8_t>> Cook(const AssetRecord& record, const std::filesystem::path& source, const Reflection::EngineObject* settings) = 0;
        virtual Core::Result<std::shared_ptr<Asset>> Load(const AssetRecord& record, const uint8_t* data, size_t size) = 0;
        virtual const Reflection::TypeInfo* GetSettingsType() const { return nullptr; }
    };

    class JsonAssetImporter : public AssetImporter
    {
    public:
        uint32_t GetVersion() const override { return 1; }
        Core::Result<std::vector<uint8_t>> Cook(const AssetRecord& record, const std::filesystem::path& source, const Reflection::EngineObject* settings) override;
        Core::Result<std::shared_ptr<Asset>> Load(const AssetRecord& record, const uint8_t* data, size_t size) override;

    protected:
        virtual Core::Result<std::shared_ptr<Asset>> CreateAsset(Serialization::JsonDocument&& document);
    };

    class TextAssetImporter : public AssetImporter
    {
    public:
        uint32_t GetVersion() const override { return 1; }
        Core::Result<std::vector<uint8_t>> Cook(const AssetRecord& record, const std::filesystem::path& source, const Reflection::EngineObject* settings) override;
        Core::Result<std::shared_ptr<Asset>> Load(const AssetRecord& record, const uint8_t* data, size_t size) override;
    };
}
