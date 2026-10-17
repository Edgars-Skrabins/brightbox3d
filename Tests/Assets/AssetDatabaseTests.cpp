#include "Assets/AssetDatabase.h"
#include "Assets/JsonAsset.h"
#include "Common/TestUtilities.h"
#include "Core/FileSystem.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace BrightBox3D;
using namespace BrightBox3D::Assets;

namespace
{
    class AssetProject
    {
    public:
        AssetProject()
        {
            m_root = Tests::MakeTempDirectory("AssetProject");
            std::filesystem::create_directories(Assets());
        }

        ~AssetProject()
        {
            AssetDatabase::Get().Close();
            std::error_code error;
            std::filesystem::remove_all(m_root, error);
        }

        bool Open() const { return AssetDatabase::Get().Open(m_root); }
        std::filesystem::path Assets() const { return m_root / AssetDatabase::AssetsFolder; }

        bool Write(const std::string& relativePath, const std::string& text) const
        {
            std::filesystem::path path = Assets() / relativePath;
            std::filesystem::create_directories(path.parent_path());
            return Core::FileSystem::WriteText(path, text);
        }

        Core::Guid GuidOf(const std::string& relativePath) const
        {
            const AssetRecord* record = AssetDatabase::Get().FindRecordByPath(relativePath);
            return record ? record->guid : Core::Guid{};
        }

    private:
        std::filesystem::path m_root;
    };

    bool Contains(const std::vector<Core::Guid>& guids, const Core::Guid& guid)
    {
        return std::find(guids.begin(), guids.end(), guid) != guids.end();
    }
}

TEST(AssetDatabaseTests, IgnoresMetaTempAndHiddenFiles)
{
    EXPECT_TRUE(AssetDatabase::IsIgnoredPath("Styles/Main.css.meta"));
    EXPECT_TRUE(AssetDatabase::IsIgnoredPath("Styles/Main.css.tmp"));
    EXPECT_TRUE(AssetDatabase::IsIgnoredPath("Styles/.DS_Store"));
    EXPECT_TRUE(AssetDatabase::IsIgnoredPath(".git"));
    EXPECT_FALSE(AssetDatabase::IsIgnoredPath("Styles/Main.css"));
    EXPECT_FALSE(AssetDatabase::IsIgnoredPath("Styles/meta.css"));

    AssetProject project;
    ASSERT_TRUE(project.Write("Styles/Main.css", "a {}"));
    ASSERT_TRUE(project.Write("Styles/Main.css.tmp", "a {}"));
    ASSERT_TRUE(project.Write(".hidden/Secret.css", "a {}"));
    ASSERT_TRUE(project.Open());

    EXPECT_TRUE(project.GuidOf("Styles/Main.css").IsValid());
    EXPECT_TRUE(project.GuidOf("Styles").IsValid());
    EXPECT_FALSE(project.GuidOf("Styles/Main.css.tmp").IsValid());
    EXPECT_FALSE(project.GuidOf(".hidden/Secret.css").IsValid());
    EXPECT_EQ(AssetDatabase::Get().GetRecords(AssetType::StyleSheet).size(), 1u);
    EXPECT_TRUE(std::filesystem::exists(project.Assets() / "Styles" / "Main.css.meta"));
    EXPECT_FALSE(std::filesystem::exists(project.Assets() / "Styles" / "Main.css.tmp.meta"));
    EXPECT_FALSE(std::filesystem::exists(project.Assets() / ".hidden" / "Secret.css.meta"));
}

TEST(AssetDatabaseTests, GuidsSurviveReopeningTheProject)
{
    AssetProject project;
    ASSERT_TRUE(project.Write("Main.css", "a {}"));
    ASSERT_TRUE(project.Open());
    Core::Guid guid = project.GuidOf("Main.css");
    ASSERT_TRUE(guid.IsValid());

    AssetDatabase::Get().Close();
    EXPECT_FALSE(AssetDatabase::Get().IsOpen());
    ASSERT_TRUE(project.Open());
    EXPECT_EQ(project.GuidOf("Main.css"), guid);
    EXPECT_EQ(AssetDatabase::Get().FindRecord(guid)->type, AssetType::StyleSheet);
}

TEST(AssetDatabaseTests, MovedAssetKeepsGuidAndRefusesToOverwrite)
{
    AssetProject project;
    ASSERT_TRUE(project.Write("A.css", "a {}"));
    ASSERT_TRUE(project.Write("B.css", "b {}"));
    ASSERT_TRUE(project.Open());
    AssetDatabase& database = AssetDatabase::Get();
    Core::Guid moved = project.GuidOf("A.css");
    Core::Guid other = project.GuidOf("B.css");

    ASSERT_TRUE(database.MoveAsset(moved, "Styles/Renamed.css"));
    EXPECT_EQ(database.FindRecord(moved)->path, "Styles/Renamed.css");
    EXPECT_EQ(database.FindRecordByPath("A.css"), nullptr);
    EXPECT_FALSE(std::filesystem::exists(project.Assets() / "A.css"));
    EXPECT_FALSE(std::filesystem::exists(project.Assets() / "A.css.meta"));
    EXPECT_TRUE(std::filesystem::exists(project.Assets() / "Styles" / "Renamed.css.meta"));

    EXPECT_FALSE(database.MoveAsset(moved, "B.css"));
    EXPECT_EQ(project.GuidOf("B.css"), other);
    EXPECT_FALSE(database.MoveAsset(Core::Guid::Generate(), "C.css"));

    database.Close();
    ASSERT_TRUE(project.Open());
    EXPECT_EQ(project.GuidOf("Styles/Renamed.css"), moved);
}

TEST(AssetDatabaseTests, CopiedMetaGetsNewGuidButExternalMovesKeepTheirs)
{
    AssetProject project;
    ASSERT_TRUE(project.Write("Original.css", "a {}"));
    ASSERT_TRUE(project.Write("Old/Child.css", "a {}"));
    ASSERT_TRUE(project.Open());
    AssetDatabase& database = AssetDatabase::Get();
    Core::Guid original = project.GuidOf("Original.css");
    Core::Guid folder = project.GuidOf("Old");
    Core::Guid child = project.GuidOf("Old/Child.css");

    std::filesystem::copy_file(project.Assets() / "Original.css", project.Assets() / "Copy.css");
    std::filesystem::copy_file(project.Assets() / "Original.css.meta", project.Assets() / "Copy.css.meta");
    std::filesystem::rename(project.Assets() / "Old", project.Assets() / "New");
    std::filesystem::rename(project.Assets() / "Old.meta", project.Assets() / "New.meta");
    database.Refresh();

    EXPECT_EQ(project.GuidOf("Original.css"), original);
    Core::Guid copy = project.GuidOf("Copy.css");
    EXPECT_TRUE(copy.IsValid());
    EXPECT_NE(copy, original);
    EXPECT_EQ(project.GuidOf("New"), folder);
    EXPECT_EQ(project.GuidOf("New/Child.css"), child);
    EXPECT_EQ(database.FindRecordByPath("Old/Child.css"), nullptr);
}

TEST(AssetDatabaseTests, DeletedAssetRemovesFileMetaAndRecord)
{
    AssetProject project;
    ASSERT_TRUE(project.Write("Doomed.css", "a {}"));
    ASSERT_TRUE(project.Write("Folder/Inner.css", "a {}"));
    ASSERT_TRUE(project.Open());
    AssetDatabase& database = AssetDatabase::Get();
    Core::Guid file = project.GuidOf("Doomed.css");
    Core::Guid folder = project.GuidOf("Folder");
    Core::Guid inner = project.GuidOf("Folder/Inner.css");

    ASSERT_TRUE(database.DeleteAsset(file));
    EXPECT_EQ(database.FindRecord(file), nullptr);
    EXPECT_FALSE(std::filesystem::exists(project.Assets() / "Doomed.css"));
    EXPECT_FALSE(std::filesystem::exists(project.Assets() / "Doomed.css.meta"));

    ASSERT_TRUE(database.DeleteAsset(folder));
    EXPECT_EQ(database.FindRecord(folder), nullptr);
    EXPECT_EQ(database.FindRecord(inner), nullptr);
    EXPECT_FALSE(std::filesystem::exists(project.Assets() / "Folder"));
    EXPECT_FALSE(database.DeleteAsset(file));
}

TEST(AssetDatabaseTests, DuplicatedAssetsGetNumberedCopiesWithNewGuids)
{
    AssetProject project;
    ASSERT_TRUE(project.Write("Styles/Main.css", "a { color: red; }"));
    ASSERT_TRUE(project.Write("Styles/Nested/Inner.css", "b {}"));
    ASSERT_TRUE(project.Open());
    AssetDatabase& database = AssetDatabase::Get();
    Core::Guid original = project.GuidOf("Styles/Main.css");

    Core::Guid copy = database.DuplicateAsset(original);
    ASSERT_TRUE(copy.IsValid());
    EXPECT_NE(copy, original);
    EXPECT_EQ(database.FindRecord(copy)->path, "Styles/Main 1.css");
    EXPECT_EQ(Core::FileSystem::ReadText(project.Assets() / "Styles" / "Main 1.css"), "a { color: red; }");
    EXPECT_EQ(project.GuidOf("Styles/Main.css"), original);
    EXPECT_EQ(database.FindRecordByPath("Styles/Main 2.css"), nullptr);
    EXPECT_EQ(database.FindRecord(database.DuplicateAsset(original))->path, "Styles/Main 2.css");

    Core::Guid folder = project.GuidOf("Styles");
    Core::Guid folderCopy = database.DuplicateAsset(folder);
    ASSERT_TRUE(folderCopy.IsValid());
    EXPECT_EQ(database.FindRecord(folderCopy)->path, "Styles 1");
    Core::Guid innerCopy = project.GuidOf("Styles 1/Nested/Inner.css");
    Core::Guid mainCopy = project.GuidOf("Styles 1/Main.css");
    ASSERT_TRUE(innerCopy.IsValid());
    ASSERT_TRUE(mainCopy.IsValid());
    EXPECT_NE(innerCopy, project.GuidOf("Styles/Nested/Inner.css"));
    EXPECT_NE(mainCopy, original);
    EXPECT_NE(project.GuidOf("Styles 1/Nested"), project.GuidOf("Styles/Nested"));

    database.Close();
    ASSERT_TRUE(project.Open());
    EXPECT_EQ(project.GuidOf("Styles 1/Main.css"), mainCopy);
    EXPECT_EQ(project.GuidOf("Styles/Main.css"), original);
    EXPECT_FALSE(database.DuplicateAsset(Core::Guid::Generate()).IsValid());
}

TEST(AssetDatabaseTests, LoadFailuresCarryTypedErrors)
{
    AssetProject project;
    ASSERT_TRUE(project.Write("Main.css", "a {}"));
    ASSERT_TRUE(project.Write("Broken.material", "{ not json"));
    ASSERT_TRUE(project.Open());
    AssetDatabase& database = AssetDatabase::Get();

    Core::Result<std::shared_ptr<Asset>> unset = database.LoadAsset(Core::Guid{});
    ASSERT_FALSE(unset);
    EXPECT_EQ(unset.GetError().GetId(), Core::EngineText::AssetNotSet);
    EXPECT_TRUE(IsAbsentAsset(unset.GetError()));

    Core::Guid unknown = Core::Guid::Generate();
    Core::Result<std::shared_ptr<Asset>> missing = database.LoadAsset(unknown);
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.GetError().GetId(), Core::EngineText::AssetNotFound);
    EXPECT_EQ(missing.GetError().GetArguments(), std::vector<std::string>{ unknown.ToString() });

    Core::Result<std::shared_ptr<JsonAsset>> wrongType = database.Load<JsonAsset>(project.GuidOf("Main.css"));
    ASSERT_FALSE(wrongType);
    EXPECT_EQ(wrongType.GetError().GetId(), Core::EngineText::AssetWrongType);
    EXPECT_FALSE(IsAbsentAsset(wrongType.GetError()));

    Core::Result<std::shared_ptr<Asset>> broken = database.LoadAsset(project.GuidOf("Broken.material"));
    ASSERT_FALSE(broken);
    EXPECT_EQ(broken.GetError().GetId(), Core::EngineText::AssetLoadFailed);
    EXPECT_EQ(broken.GetError().GetArguments().front(), "Broken.material");
    EXPECT_EQ(database.FindLoaded(project.GuidOf("Broken.material")), nullptr);
}

TEST(AssetDatabaseTests, RefreshBroadcastsExternalEditsAndDeletions)
{
    AssetProject project;
    ASSERT_TRUE(project.Write("Edited.css", "a {}"));
    ASSERT_TRUE(project.Write("Removed.css", "a {}"));
    ASSERT_TRUE(project.Open());
    AssetDatabase& database = AssetDatabase::Get();
    Core::Guid edited = project.GuidOf("Edited.css");
    Core::Guid removed = project.GuidOf("Removed.css");
    ASSERT_TRUE(Tests::Succeeded(database.LoadAsset(edited)));
    ASSERT_NE(database.FindLoaded(edited), nullptr);

    std::vector<Core::Guid> changedEvents;
    std::vector<Core::Guid> removedEvents;
    int refreshes = 0;
    Core::EventHandle changedHandle = database.OnAssetChanged.Subscribe([&](const Core::Guid& guid) { changedEvents.push_back(guid); });
    Core::EventHandle removedHandle = database.OnAssetRemoved.Subscribe([&](const Core::Guid& guid) { removedEvents.push_back(guid); });
    Core::EventHandle refreshedHandle = database.OnRefreshed.Subscribe([&]() { ++refreshes; });
    uint64_t revision = database.GetRevision();

    ASSERT_TRUE(project.Write("Edited.css", "a { color: red; }"));
    std::filesystem::remove(project.Assets() / "Removed.css");
    database.Refresh();

    database.OnAssetChanged.Unsubscribe(changedHandle);
    database.OnAssetRemoved.Unsubscribe(removedHandle);
    database.OnRefreshed.Unsubscribe(refreshedHandle);

    EXPECT_TRUE(Contains(changedEvents, edited));
    EXPECT_FALSE(Contains(changedEvents, removed));
    EXPECT_TRUE(Contains(removedEvents, removed));
    EXPECT_EQ(refreshes, 1);
    EXPECT_GT(database.GetRevision(), revision);
    EXPECT_EQ(database.FindLoaded(edited), nullptr);
    EXPECT_EQ(database.FindRecord(removed), nullptr);
}

TEST(AssetDatabaseTests, UniquePathsNumberCollisionsBeforeTheExtension)
{
    AssetProject project;
    ASSERT_TRUE(project.Open());
    AssetDatabase& database = AssetDatabase::Get();
    EXPECT_EQ(database.MakeUniquePath("Item.css"), "Item.css");

    ASSERT_TRUE(project.Write("Item.css", "a {}"));
    EXPECT_EQ(database.MakeUniquePath("Item.css"), "Item 1.css");
    ASSERT_TRUE(project.Write("Item 1.css", "a {}"));
    EXPECT_EQ(database.MakeUniquePath("Item.css"), "Item 2.css");

    ASSERT_TRUE(project.Write("UI/Hud.screen.xml", "<screen/>"));
    EXPECT_EQ(database.MakeUniquePath("UI/Hud.screen.xml"), "UI/Hud 1.screen.xml");
}

TEST(AssetDatabaseTests, ReferencesResolveFromGuidsAndPaths)
{
    AssetProject project;
    ASSERT_TRUE(project.Write("Styles/Main.css", "a {}"));
    ASSERT_TRUE(project.Open());
    AssetDatabase& database = AssetDatabase::Get();
    Core::Guid guid = project.GuidOf("Styles/Main.css");
    ASSERT_TRUE(guid.IsValid());

    EXPECT_EQ(database.ResolveReference(guid.ToString()), guid);
    EXPECT_EQ(database.ResolveReference("Styles/Main.css"), guid);
    EXPECT_EQ(database.ResolveReference("Assets/Styles/Main.css"), guid);
    EXPECT_FALSE(database.ResolveReference("Styles/Missing.css").IsValid());
    EXPECT_FALSE(database.ResolveReference("").IsValid());
}
