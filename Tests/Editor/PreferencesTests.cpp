#include "Common/TestUtilities.h"
#include "Core/FileSystem.h"
#include "EditorPreferences.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

using namespace BrightBox3D;
using Editor::AssetsLayout;
using Editor::EditorPreferences;
using Editor::TerminalFilter;

namespace
{
    class PreferencesTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_path = Tests::MakeTempDirectory("Preferences") / "Preferences.json";
        }

        void TearDown() override
        {
            EditorPreferences::Get().Load(Tests::MakeTempDirectory("PreferencesReset") / "Preferences.json");
            std::error_code error;
            std::filesystem::remove_all(m_path.parent_path(), error);
        }

        void LoadText(const std::string& text)
        {
            ASSERT_TRUE(Core::FileSystem::WriteText(m_path, text));
            EditorPreferences::Get().Load(m_path);
        }

        std::filesystem::path m_path;
    };
}

TEST_F(PreferencesTests, MissingFileUsesAssetDefaults)
{
    EditorPreferences& preferences = EditorPreferences::Get();
    preferences.Load(m_path);
    EXPECT_EQ(preferences.GetAssetsLayout(), EditorPreferences::DefaultAssetsLayout);
    EXPECT_EQ(preferences.GetAssetIconSize(), EditorPreferences::DefaultAssetIconSize);
    EXPECT_FALSE(preferences.IsAssetListMode());
}

TEST_F(PreferencesTests, AssetLayoutAndIconSizeRoundTrip)
{
    EditorPreferences& preferences = EditorPreferences::Get();
    preferences.Load(m_path);
    preferences.SetAssetsLayout(AssetsLayout::OneColumn);
    preferences.SetAssetIconSize(120);
    preferences.Save();

    preferences.SetAssetsLayout(AssetsLayout::TwoColumn);
    preferences.SetAssetIconSize(EditorPreferences::DefaultAssetIconSize);
    preferences.Load(m_path);
    EXPECT_EQ(preferences.GetAssetsLayout(), AssetsLayout::OneColumn);
    EXPECT_EQ(preferences.GetAssetIconSize(), 120);

    preferences.SetAssetIconSize(EditorPreferences::AssetListIconSize);
    preferences.Save();
    preferences.Load(m_path);
    EXPECT_TRUE(preferences.IsAssetListMode());
}

TEST_F(PreferencesTests, IconSizeSetterSnapsToListOrRange)
{
    EditorPreferences& preferences = EditorPreferences::Get();
    preferences.SetAssetIconSize(-5);
    EXPECT_TRUE(preferences.IsAssetListMode());
    preferences.SetAssetIconSize(10);
    EXPECT_EQ(preferences.GetAssetIconSize(), EditorPreferences::MinAssetIconSize);
    preferences.SetAssetIconSize(999);
    EXPECT_EQ(preferences.GetAssetIconSize(), EditorPreferences::MaxAssetIconSize);
}

TEST_F(PreferencesTests, InvalidAssetValuesFallBackToDefaults)
{
    LoadText(R"({ "assets": { "layout": "ThreeColumn", "iconSize": 30 } })");
    EXPECT_EQ(EditorPreferences::Get().GetAssetsLayout(), EditorPreferences::DefaultAssetsLayout);
    EXPECT_EQ(EditorPreferences::Get().GetAssetIconSize(), EditorPreferences::DefaultAssetIconSize);

    LoadText(R"({ "assets": { "layout": 3, "iconSize": "large" } })");
    EXPECT_EQ(EditorPreferences::Get().GetAssetsLayout(), EditorPreferences::DefaultAssetsLayout);
    EXPECT_EQ(EditorPreferences::Get().GetAssetIconSize(), EditorPreferences::DefaultAssetIconSize);

    LoadText(R"({ "assets": "broken" })");
    EXPECT_EQ(EditorPreferences::Get().GetAssetIconSize(), EditorPreferences::DefaultAssetIconSize);

    LoadText(R"({ "assets": { "layout": "OneColumn", "iconSize": 0 } })");
    EXPECT_EQ(EditorPreferences::Get().GetAssetsLayout(), AssetsLayout::OneColumn);
    EXPECT_TRUE(EditorPreferences::Get().IsAssetListMode());
}

TEST_F(PreferencesTests, TerminalFiltersDefaultOnAndRoundTrip)
{
    EditorPreferences& preferences = EditorPreferences::Get();
    preferences.Load(m_path);
    for (int index = 0; index < static_cast<int>(TerminalFilter::Count); ++index)
    {
        EXPECT_TRUE(preferences.IsTerminalFilterOn(static_cast<TerminalFilter>(index)));
    }
    preferences.SetTerminalFilter(TerminalFilter::Engine, false);
    preferences.SetTerminalFilter(TerminalFilter::Warnings, false);
    preferences.Save();

    preferences.SetTerminalFilter(TerminalFilter::Engine, true);
    preferences.Load(m_path);
    EXPECT_TRUE(preferences.IsTerminalFilterOn(TerminalFilter::Info));
    EXPECT_FALSE(preferences.IsTerminalFilterOn(TerminalFilter::Warnings));
    EXPECT_TRUE(preferences.IsTerminalFilterOn(TerminalFilter::Errors));
    EXPECT_FALSE(preferences.IsTerminalFilterOn(TerminalFilter::Engine));

    LoadText(R"({ "terminal": { "showInfo": false, "showEngine": "no" } })");
    EXPECT_FALSE(preferences.IsTerminalFilterOn(TerminalFilter::Info));
    EXPECT_TRUE(preferences.IsTerminalFilterOn(TerminalFilter::Engine));
    EXPECT_TRUE(preferences.IsTerminalFilterOn(TerminalFilter::Warnings));
}
