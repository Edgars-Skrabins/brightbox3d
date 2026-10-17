#include "Rendering/MaterialImporter.h"

#include "Rendering/MaterialAsset.h"
#include "Serialization/ObjectSerializer.h"

namespace BrightBox3D::Rendering
{
    Core::Result<std::shared_ptr<Assets::Asset>> MaterialImporter::CreateAsset(Serialization::JsonDocument&& document)
    {
        auto material = std::make_shared<MaterialAsset>();
        Serialization::DeserializeProperties(*material, document);
        return material;
    }
}
