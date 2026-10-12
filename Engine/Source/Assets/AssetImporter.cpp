#include "Assets/AssetImporter.h"

#include "Assets/JsonAsset.h"
#include "Assets/TextAsset.h"
#include "Core/FileSystem.h"
#include "Serialization/BinaryJson.h"

namespace BrightBox3D::Assets
{
    bool JsonAssetImporter::Cook(const AssetRecord& record, const std::filesystem::path& source, const Serialization::JsonValue& settings, std::vector<uint8_t>& output, std::string& error)
    {
        Serialization::JsonDocument document;
        if (!Serialization::LoadJsonFile(source, document, &error))
        {
            return false;
        }
        output = Serialization::EncodeBinaryJson(document);
        return true;
    }

    std::shared_ptr<Asset> JsonAssetImporter::Load(const AssetRecord& record, const uint8_t* data, size_t size, std::string& error)
    {
        Serialization::JsonDocument document;
        if (!Serialization::DecodeBinaryJson(data, size, document))
        {
            error = "corrupt cooked JSON data";
            return nullptr;
        }
        return CreateAsset(std::move(document), error);
    }

    std::shared_ptr<Asset> JsonAssetImporter::CreateAsset(Serialization::JsonDocument&& document, std::string& error)
    {
        auto asset = std::make_shared<JsonAsset>();
        asset->GetDocument().Swap(document);
        return asset;
    }

    bool TextAssetImporter::Cook(const AssetRecord& record, const std::filesystem::path& source, const Serialization::JsonValue& settings, std::vector<uint8_t>& output, std::string& error)
    {
        auto text = Core::FileSystem::ReadText(source);
        if (!text)
        {
            error = "cannot read file";
            return false;
        }
        output.assign(text->begin(), text->end());
        return true;
    }

    std::shared_ptr<Asset> TextAssetImporter::Load(const AssetRecord& record, const uint8_t* data, size_t size, std::string& error)
    {
        auto asset = std::make_shared<TextAsset>();
        asset->SetText(std::string(reinterpret_cast<const char*>(data), size));
        return asset;
    }
}
