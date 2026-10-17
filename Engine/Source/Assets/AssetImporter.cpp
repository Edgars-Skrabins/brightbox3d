#include "Assets/AssetImporter.h"

#include "Assets/JsonAsset.h"
#include "Assets/TextAsset.h"
#include "Core/EngineText.h"
#include "Core/FileSystem.h"
#include "Serialization/BinaryJson.h"

namespace BrightBox3D::Assets
{
    Core::Result<std::vector<uint8_t>> JsonAssetImporter::Cook(const AssetRecord& record, const std::filesystem::path& source, const Reflection::EngineObject* settings)
    {
        Serialization::JsonDocument document;
        if (Core::Result<void> loaded = Serialization::LoadJsonFile(source, document); !loaded)
        {
            return loaded.GetError();
        }
        return Serialization::EncodeBinaryJson(document);
    }

    Core::Result<std::shared_ptr<Asset>> JsonAssetImporter::Load(const AssetRecord& record, const uint8_t* data, size_t size)
    {
        Serialization::JsonDocument document;
        if (!Serialization::DecodeBinaryJson(data, size, document))
        {
            return Core::Error(Core::EngineText::CorruptCookedJson);
        }
        return CreateAsset(std::move(document));
    }

    Core::Result<std::shared_ptr<Asset>> JsonAssetImporter::CreateAsset(Serialization::JsonDocument&& document)
    {
        auto asset = std::make_shared<JsonAsset>();
        asset->GetDocument().Swap(document);
        return asset;
    }

    Core::Result<std::vector<uint8_t>> TextAssetImporter::Cook(const AssetRecord& record, const std::filesystem::path& source, const Reflection::EngineObject* settings)
    {
        auto text = Core::FileSystem::ReadText(source);
        if (!text)
        {
            return Core::Error(Core::EngineText::CannotReadFile);
        }
        return std::vector<uint8_t>(text->begin(), text->end());
    }

    Core::Result<std::shared_ptr<Asset>> TextAssetImporter::Load(const AssetRecord& record, const uint8_t* data, size_t size)
    {
        auto asset = std::make_shared<TextAsset>();
        asset->SetText(std::string(reinterpret_cast<const char*>(data), size));
        return asset;
    }
}
