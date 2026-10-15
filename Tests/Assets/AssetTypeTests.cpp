#include "Assets/AssetType.h"

#include <gtest/gtest.h>

#include <cctype>
#include <string>

using namespace BrightBox3D::Assets;

TEST(AssetTypeTests, EveryExtensionMapsToItsType)
{
    for (int index = static_cast<int>(AssetType::Level); index < static_cast<int>(AssetType::Count); ++index)
    {
        AssetType type = static_cast<AssetType>(index);
        for (const char* extension : AssetTypeExtensions(type))
        {
            EXPECT_EQ(AssetTypeFromPath(std::string("Folder/Name") + extension), type) << extension;
            std::string upper = std::string("NAME") + extension;
            for (char& c : upper)
            {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            EXPECT_EQ(AssetTypeFromPath(upper), type) << upper;
        }
    }
}

TEST(AssetTypeTests, CompoundExtensionsWinAndUnknownFilesStayUnknown)
{
    EXPECT_EQ(AssetTypeFromPath("UI/Hud.screen.xml"), AssetType::Screen);
    EXPECT_EQ(AssetTypeFromPath("UI/Card.widget.xml"), AssetType::Widget);
    EXPECT_EQ(AssetTypeFromPath("UI/Data.xml"), AssetType::Unknown);
    EXPECT_EQ(AssetTypeFromPath("Notes.txt"), AssetType::Unknown);
    EXPECT_EQ(AssetTypeFromPath("Folder"), AssetType::Unknown);
}

TEST(AssetTypeTests, NamesRoundTrip)
{
    for (int index = 0; index < static_cast<int>(AssetType::Count); ++index)
    {
        AssetType type = static_cast<AssetType>(index);
        EXPECT_EQ(AssetTypeFromName(AssetTypeName(type)), type);
    }
    EXPECT_STREQ(AssetTypeExtension(AssetType::Texture), ".png");
}

TEST(AssetTypeTests, MatchedExtensionKeepsTheFileCaseAndCompoundExtensions)
{
    EXPECT_EQ(MatchedExtension(AssetType::Screen, "Hud.screen.xml"), ".screen.xml");
    EXPECT_EQ(MatchedExtension(AssetType::Texture, "Photo.JPEG"), ".JPEG");
    EXPECT_EQ(MatchedExtension(AssetType::Material, "Crate.v2.material"), ".material");
    EXPECT_EQ(MatchedExtension(AssetType::Material, "Crate.png"), "");
    EXPECT_EQ(MatchedExtension(AssetType::Folder, "Folder.With.Dots"), "");
}

TEST(AssetTypeTests, OnlyCreatableTypesHaveNewAssetNames)
{
    EXPECT_TRUE(IsCreatableAssetType(AssetType::Folder));
    EXPECT_TRUE(IsCreatableAssetType(AssetType::Material));
    EXPECT_TRUE(IsCreatableAssetType(AssetType::StyleSheet));
    EXPECT_FALSE(IsCreatableAssetType(AssetType::Model));
    EXPECT_FALSE(IsCreatableAssetType(AssetType::Texture));
    EXPECT_FALSE(IsCreatableAssetType(AssetType::LevelObjectVariant));
    EXPECT_EQ(NewAssetName(AssetType::Material), "New Material");
    EXPECT_EQ(NewAssetName(AssetType::Model), AssetTypeName(AssetType::Model));
}
