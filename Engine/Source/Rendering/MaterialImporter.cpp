#include "Rendering/MaterialImporter.h"

#include "Rendering/MaterialAsset.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Rendering
{
    std::shared_ptr<Assets::Asset> MaterialImporter::CreateAsset(Serialization::JsonDocument&& document, std::string& error)
    {
        auto material = std::make_shared<MaterialAsset>();
        Serialization::DeserializeProperties(*material, document);
        return material;
    }
}
