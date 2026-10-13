#include "UI/UIAssets.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Core/FileSystem.h"

namespace BrightBox3D::UI
{
    namespace
    {
        template <typename T>
        class TypedTextImporter : public Assets::TextAssetImporter
        {
        public:
            std::shared_ptr<Assets::Asset> Load(const Assets::AssetRecord& record, const uint8_t* data, size_t size, std::string& error) override
            {
                auto asset = std::make_shared<T>();
                asset->SetText(std::string(reinterpret_cast<const char*>(data), size));
                return asset;
            }
        };

        class FontImporter : public Assets::AssetImporter
        {
        public:
            uint32_t GetVersion() const override { return 1; }

            bool Cook(const Assets::AssetRecord& record, const std::filesystem::path& source, const Serialization::JsonValue& settings, std::vector<uint8_t>& output, std::string& error) override
            {
                FontData data;
                if (!GenerateFontData(source, data, error))
                {
                    return false;
                }
                output = data.Serialize();
                return true;
            }

            std::shared_ptr<Assets::Asset> Load(const Assets::AssetRecord& record, const uint8_t* data, size_t size, std::string& error) override
            {
                auto asset = std::make_shared<FontAsset>();
                if (!asset->GetData().Deserialize(data, size))
                {
                    error = Core::Text(Core::EngineText::CorruptCookedFont);
                    return nullptr;
                }
                return asset;
            }
        };

        struct Registration
        {
            Registration()
            {
                RegisterUIAssets();
            }
        };

        Registration s_registration;
    }

    void RegisterUIAssets()
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        database.RegisterImporter(Assets::AssetType::Screen, std::make_shared<TypedTextImporter<ScreenAsset>>());
        database.RegisterImporter(Assets::AssetType::Widget, std::make_shared<TypedTextImporter<WidgetAsset>>());
        database.RegisterImporter(Assets::AssetType::StyleSheet, std::make_shared<TypedTextImporter<StyleSheetAsset>>());
        database.RegisterImporter(Assets::AssetType::Font, std::make_shared<FontImporter>());
    }
}
