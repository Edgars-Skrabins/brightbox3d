#include "Common/TestUtilities.h"
#include "Core/FileSystem.h"
#include "Scripts/ScriptTemplate.h"
#include "Scripts/ScriptFiles.h"
#include "Services/ScriptLibrary.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace BrightBox3D;
using Editor::ScriptEntry;
using Editor::ScriptLibrary;

namespace
{
    class ScriptLibraryTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_root = Tests::MakeTempDirectory("Scripts");
            m_library.SetRoot(m_root);
        }

        void TearDown() override
        {
            std::error_code error;
            std::filesystem::remove_all(m_root, error);
        }

        void Write(const std::string& relativePath)
        {
            std::filesystem::path path = m_root / relativePath;
            std::filesystem::create_directories(path.parent_path());
            Core::FileSystem::WriteText(path, "// " + relativePath);
        }

        std::vector<std::string> ClassesIn(const std::string& folder)
        {
            std::vector<std::string> names;
            for (const ScriptEntry& entry : m_library.ScriptsIn(folder))
            {
                names.push_back(entry.className);
            }
            return names;
        }

        std::filesystem::path m_root;
        ScriptLibrary m_library;
    };
}

TEST_F(ScriptLibraryTests, PairsHeadersAndSourcesIntoOneScript)
{
    Write("Spinner.h");
    Write("Spinner.cpp");
    Write("Categories.h");
    Write("Notes.txt");
    Write(".hidden/Secret.h");
    Write("Enemies/Grunt.h");
    Write("Enemies/Grunt.cpp");
    m_library.Refresh();

    EXPECT_EQ(ClassesIn(""), (std::vector<std::string>{ "Categories", "Spinner" }));
    EXPECT_EQ(ClassesIn("Enemies"), std::vector<std::string>{ "Grunt" });
    EXPECT_EQ(m_library.ChildFolders(""), std::vector<std::string>{ "Enemies" });

    const ScriptEntry* spinner = m_library.FindByClass("Spinner");
    ASSERT_NE(spinner, nullptr);
    EXPECT_EQ(spinner->header.filename(), "Spinner.h");
    EXPECT_EQ(spinner->source.filename(), "Spinner.cpp");
    EXPECT_EQ(spinner->PrimaryFile().filename(), "Spinner.cpp");
    EXPECT_EQ(m_library.FindByClass("Categories")->PrimaryFile().filename(), "Categories.h");
    EXPECT_EQ(m_library.Find(spinner->id), spinner);
    EXPECT_EQ(m_library.FindFolder(ScriptLibrary::FolderId("Enemies")), std::optional<std::string>("Enemies"));
}

TEST_F(ScriptLibraryTests, IdsStayStableAcrossRescans)
{
    Write("Spinner.h");
    m_library.Refresh();
    Core::Guid id = m_library.FindByClass("Spinner")->id;
    Write("Other.h");
    m_library.Refresh();
    EXPECT_EQ(m_library.FindByClass("Spinner")->id, id);
}

TEST_F(ScriptLibraryTests, FoldersAreCreatedRenamedAndMoved)
{
    Core::Result<std::string, Editor::EditorError> first = Editor::ScriptFiles::CreateFolder(m_root, "");
    Core::Result<std::string, Editor::EditorError> second = Editor::ScriptFiles::CreateFolder(m_root, "");
    ASSERT_TRUE(Tests::Succeeded(first));
    ASSERT_TRUE(Tests::Succeeded(second));
    EXPECT_NE(*first, *second);

    Core::Result<std::string, Editor::EditorError> renamed = Editor::ScriptFiles::RenameFolder(m_root, *first, "Enemies");
    ASSERT_TRUE(Tests::Succeeded(renamed));
    EXPECT_EQ(*renamed, "Enemies");
    EXPECT_TRUE(std::filesystem::is_directory(m_root / "Enemies"));

    Core::Result<std::string, Editor::EditorError> moved = Editor::ScriptFiles::MoveFolder(m_root, *second, "Enemies");
    ASSERT_TRUE(Tests::Succeeded(moved));
    EXPECT_EQ(*moved, "Enemies/" + *second);

    Core::Result<std::string, Editor::EditorError> intoItself = Editor::ScriptFiles::MoveFolder(m_root, "Enemies", *moved);
    ASSERT_TRUE(Tests::Succeeded(intoItself));
    EXPECT_EQ(*intoItself, "Enemies");

    Write("Enemies/Taken/Keep.h");
    Core::Result<std::string, Editor::EditorError> taken = Editor::ScriptFiles::RenameFolder(m_root, *moved, "Taken");
    ASSERT_FALSE(taken);
    EXPECT_EQ(taken.GetError().GetId(), Editor::EditorText::ScriptPathExists);
}

TEST_F(ScriptLibraryTests, MovingAScriptMovesBothFiles)
{
    Write("Spinner.h");
    Write("Spinner.cpp");
    m_library.Refresh();
    ScriptEntry spinner = *m_library.FindByClass("Spinner");
    ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::MoveScript(m_root, spinner, "Gameplay/Props")));
    m_library.Refresh();
    EXPECT_TRUE(ClassesIn("").empty());
    EXPECT_EQ(ClassesIn("Gameplay/Props"), std::vector<std::string>{ "Spinner" });
    EXPECT_FALSE(m_library.FindByClass("Spinner")->header.empty());
    EXPECT_FALSE(m_library.FindByClass("Spinner")->source.empty());
}

TEST_F(ScriptLibraryTests, RenamingRewritesTheClassAndAddsARedirect)
{
    ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::CreateScript(m_root, "", "Rotator", Editor::ScriptTemplate::Component)));
    m_library.Refresh();
    ScriptEntry rotator = *m_library.FindByClass("Rotator");
    ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::RenameScript(rotator, "Spinner", m_library)));
    m_library.Refresh();
    EXPECT_EQ(m_library.FindByClass("Rotator"), nullptr);
    const ScriptEntry* spinner = m_library.FindByClass("Spinner");
    ASSERT_NE(spinner, nullptr);
    std::string header = *Core::FileSystem::ReadText(spinner->header);
    std::string source = *Core::FileSystem::ReadText(spinner->source);
    EXPECT_NE(header.find("class Spinner : public"), std::string::npos);
    EXPECT_EQ(header.find("Rotator"), std::string::npos);
    EXPECT_NE(source.find("#include \"Spinner.h\""), std::string::npos);
    EXPECT_NE(source.find("void Spinner::OnGameUpdate"), std::string::npos);
    EXPECT_NE(source.find("TypeRedirectRegistration<Spinner> s_redirectFromRotator(\"Rotator\")"), std::string::npos);

    Core::Result<void, Editor::EditorError> invalid = Editor::ScriptFiles::RenameScript(*spinner, "Not Valid", m_library);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.GetError().GetId(), Editor::EditorText::ScriptNameInvalid);
}

TEST_F(ScriptLibraryTests, AnEmptyOrMissingRootHasNoScripts)
{
    m_library.SetRoot({});
    EXPECT_TRUE(m_library.GetScripts().empty());
    EXPECT_TRUE(m_library.GetFolders().empty());
    m_library.SetRoot(m_root / "Missing");
    EXPECT_TRUE(m_library.GetScripts().empty());
    EXPECT_TRUE(m_library.HasFolder(""));
    EXPECT_FALSE(m_library.HasFolder("Nope"));
}

TEST_F(ScriptLibraryTests, ClassNamesMustBeIdentifiers)
{
    for (const char* valid : { "Spinner", "_Hidden", "Player2", "a" })
    {
        EXPECT_TRUE(Editor::ScriptFiles::IsClassName(valid)) << valid;
    }
    for (const char* invalid : { "", "2Fast", "My Script", "Bad-Name", "Dot.h" })
    {
        EXPECT_FALSE(Editor::ScriptFiles::IsClassName(invalid)) << invalid;
    }
}

TEST_F(ScriptLibraryTests, EveryTemplateRendersAHeaderAndSourceForItsBase)
{
    for (size_t index = 0; index < static_cast<size_t>(Editor::ScriptTemplate::Count); ++index)
    {
        Editor::ScriptTemplate scriptTemplate = static_cast<Editor::ScriptTemplate>(index);
        const Editor::ScriptTemplateInfo& info = Editor::GetScriptTemplateInfo(scriptTemplate);
        Core::Result<Editor::ScriptSources, Editor::EditorError> sources = Editor::RenderScriptTemplate(scriptTemplate, "Sample");
        ASSERT_TRUE(Tests::Succeeded(sources)) << info.fileStem;
        EXPECT_NE(sources->header.find("class Sample : public"), std::string::npos) << info.fileStem;
        EXPECT_NE(sources->header.find(info.baseType().GetName()), std::string::npos) << info.fileStem;
        EXPECT_NE(sources->source.find("#include \"Sample.h\""), std::string::npos) << info.fileStem;
        EXPECT_EQ(sources->header.find("@"), std::string::npos) << info.fileStem;
        EXPECT_EQ(sources->source.find("@"), std::string::npos) << info.fileStem;
    }
}

TEST_F(ScriptLibraryTests, CreatingAScriptWritesBothFilesIntoItsFolder)
{
    Core::Result<std::filesystem::path, Editor::EditorError> source = Editor::ScriptFiles::CreateScript(m_root, "Gameplay/Props", "Spinner", Editor::ScriptTemplate::Component);
    ASSERT_TRUE(Tests::Succeeded(source));
    EXPECT_EQ(*source, m_root / "Gameplay/Props/Spinner.cpp");
    EXPECT_TRUE(std::filesystem::exists(m_root / "Gameplay/Props/Spinner.h"));
    m_library.Refresh();
    EXPECT_EQ(ClassesIn("Gameplay/Props"), std::vector<std::string>{ "Spinner" });

    Core::Result<std::filesystem::path, Editor::EditorError> again = Editor::ScriptFiles::CreateScript(m_root, "Gameplay/Props", "Spinner", Editor::ScriptTemplate::Component);
    ASSERT_FALSE(again);
    EXPECT_EQ(again.GetError().GetId(), Editor::EditorText::ScriptPathExists);
    Core::Result<void, Editor::EditorError> taken = Editor::ScriptFiles::ValidateNewClassName("Spinner", m_library);
    ASSERT_FALSE(taken);
    EXPECT_EQ(taken.GetError().GetId(), Editor::EditorText::ScriptNameTaken);
}

TEST_F(ScriptLibraryTests, RenamingKeepsTheFolderAndLeavesOtherScriptsAlone)
{
    ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::CreateScript(m_root, "Enemies", "Grunt", Editor::ScriptTemplate::Character)));
    Write("Spawner.h");
    Core::FileSystem::WriteText(m_root / "Spawner.h", "#include \"Enemies/Grunt.h\"\nGrunt* m_grunt;\n");
    m_library.Refresh();
    ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::RenameScript(*m_library.FindByClass("Grunt"), "Brute", m_library)));
    m_library.Refresh();
    EXPECT_EQ(ClassesIn("Enemies"), std::vector<std::string>{ "Brute" });
    EXPECT_EQ(*Core::FileSystem::ReadText(m_root / "Spawner.h"), "#include \"Enemies/Grunt.h\"\nGrunt* m_grunt;\n");

    std::string before = *Core::FileSystem::ReadText(m_library.FindByClass("Brute")->source);
    ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::RenameScript(*m_library.FindByClass("Brute"), "Brute", m_library)));
    EXPECT_EQ(*Core::FileSystem::ReadText(m_library.FindByClass("Brute")->source), before);
}

TEST_F(ScriptLibraryTests, NewFoldersGetUniqueNamesInsideTheirParent)
{
    std::vector<std::string> created;
    for (int index = 0; index < 3; ++index)
    {
        Core::Result<std::string, Editor::EditorError> folder = Editor::ScriptFiles::CreateFolder(m_root, "Gameplay");
        ASSERT_TRUE(Tests::Succeeded(folder));
        EXPECT_EQ(Editor::ScriptFiles::ParentFolder(*folder), "Gameplay");
        created.push_back(*folder);
    }
    std::sort(created.begin(), created.end());
    EXPECT_EQ(std::unique(created.begin(), created.end()), created.end());
    m_library.Refresh();
    EXPECT_EQ(m_library.ChildFolders("Gameplay").size(), 3u);
}
