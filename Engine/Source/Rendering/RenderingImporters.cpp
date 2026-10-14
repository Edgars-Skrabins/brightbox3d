#include "Rendering/RenderingImporters.h"

#include "Assets/AssetDatabase.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/MaterialImporter.h"
#include "Rendering/TextureImporter.h"

#include <memory>

namespace BrightBox3D::Rendering
{
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
