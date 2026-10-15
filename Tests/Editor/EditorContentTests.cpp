#include "Assets/AssetType.h"
#include "Core/FileSystem.h"
#include "EditorContent.h"
#include "EditorLayout.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace BrightBox3D;

TEST(EditorContentTests, StarterTemplatesExistForEveryTextAssetType)
{
    std::filesystem::path directory = Editor::EditorContentDirectory();
    ASSERT_FALSE(directory.empty());
    for (Assets::AssetType type : { Assets::AssetType::Screen, Assets::AssetType::Widget, Assets::AssetType::StyleSheet })
    {
        std::filesystem::path file = directory / Editor::EditorLayout::AssetTemplatesFolder / (std::string(Assets::AssetTypeName(type)) + Assets::AssetTypeExtension(type));
        std::optional<std::string> content = Core::FileSystem::ReadText(file);
        ASSERT_TRUE(content.has_value()) << file;
        EXPECT_FALSE(content->empty()) << file;
        EXPECT_EQ(Assets::AssetTypeFromPath(file), type) << file;
    }
}
