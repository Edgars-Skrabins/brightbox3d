#include "Commands/CommandRegistry.h"
#include "Commands/KeyChord.h"
#include "Commands/ShortcutBindings.h"
#include "Common/TestUtilities.h"
#include "EditorPreferences.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <vector>

using namespace BrightBox3D;
using Editor::CommandId;
using Editor::CommandRegistry;
using Editor::CommandScope;
using Editor::KeyChord;

namespace
{
    Editor::ShortcutInput Pressing(std::vector<KeyChord> pressed, bool textInput = false)
    {
        return { [pressed](const KeyChord& chord) { return std::find(pressed.begin(), pressed.end(), chord) != pressed.end(); }, textInput };
    }

    class CommandTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            CommandRegistry::Get().Clear();
        }

        void TearDown() override
        {
            CommandRegistry::Get().Clear();
        }

        void RegisterCounter(CommandId id, const char* key, CommandScope scope, KeyChord chord, int& counter)
        {
            CommandRegistry::Get().Register(id, { .key = key, .scope = scope, .defaultShortcut = chord, .execute = [&counter] { ++counter; } });
        }
    };
}

TEST_F(CommandTests, DispatchOnlyRunsCommandsOfThatScope)
{
    int save = 0;
    int move = 0;
    RegisterCounter(CommandId::Save, "Save", CommandScope::Global, { ImGuiKey_S, ImGuiMod_Ctrl }, save);
    RegisterCounter(CommandId::GizmoMove, "GizmoMove", CommandScope::LevelView, { ImGuiKey_W }, move);
    Editor::ShortcutInput input = Pressing({ { ImGuiKey_S, ImGuiMod_Ctrl }, { ImGuiKey_W } });

    CommandRegistry::Get().BeginFrame();
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Global, input);
    EXPECT_EQ(save, 1);
    EXPECT_EQ(move, 0);

    CommandRegistry::Get().DispatchShortcuts(CommandScope::LevelView, input);
    EXPECT_EQ(move, 1);
}

TEST_F(CommandTests, AssetsDeleteWinsOverGlobalDeleteWhileAssetsDispatch)
{
    int levelObjects = 0;
    int assets = 0;
    RegisterCounter(CommandId::Delete, "Delete", CommandScope::Global, { ImGuiKey_Delete }, levelObjects);
    RegisterCounter(CommandId::AssetDelete, "AssetDelete", CommandScope::Assets, { ImGuiKey_Delete }, assets);
    Editor::ShortcutInput input = Pressing({ { ImGuiKey_Delete } });

    CommandRegistry::Get().BeginFrame();
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Assets, input);
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Global, input);
    EXPECT_EQ(assets, 1);
    EXPECT_EQ(levelObjects, 0);

    CommandRegistry::Get().BeginFrame();
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Global, input);
    EXPECT_EQ(levelObjects, 1);
}

TEST_F(CommandTests, DisabledAssetsCommandStillKeepsTheChordFromGlobal)
{
    int levelObjects = 0;
    int assets = 0;
    RegisterCounter(CommandId::Delete, "Delete", CommandScope::Global, { ImGuiKey_Delete }, levelObjects);
    CommandRegistry::Get().Register(CommandId::AssetDelete, { .key = "AssetDelete", .scope = CommandScope::Assets, .defaultShortcut = { ImGuiKey_Delete }, .isEnabled = [] { return false; }, .execute = [&assets] { ++assets; } });
    Editor::ShortcutInput input = Pressing({ { ImGuiKey_Delete } });

    CommandRegistry::Get().BeginFrame();
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Assets, input);
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Global, input);
    EXPECT_EQ(assets, 0);
    EXPECT_EQ(levelObjects, 0);
}

TEST_F(CommandTests, PanelScopeBeatsGlobalForTheSameChord)
{
    int global = 0;
    int panel = 0;
    RegisterCounter(CommandId::Save, "Save", CommandScope::Global, { ImGuiKey_F }, global);
    RegisterCounter(CommandId::FocusSelection, "FocusSelection", CommandScope::LevelView, { ImGuiKey_F }, panel);
    Editor::ShortcutInput input = Pressing({ { ImGuiKey_F } });

    CommandRegistry::Get().BeginFrame();
    CommandRegistry::Get().DispatchShortcuts(CommandScope::LevelView, input);
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Global, input);
    EXPECT_EQ(panel, 1);
    EXPECT_EQ(global, 0);

    CommandRegistry::Get().BeginFrame();
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Global, input);
    EXPECT_EQ(global, 1);
}

TEST_F(CommandTests, NothingFiresDuringTextInput)
{
    int save = 0;
    RegisterCounter(CommandId::Save, "Save", CommandScope::Global, { ImGuiKey_S, ImGuiMod_Ctrl }, save);
    CommandRegistry::Get().BeginFrame();
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Global, Pressing({ { ImGuiKey_S, ImGuiMod_Ctrl } }, true));
    EXPECT_EQ(save, 0);
}

TEST_F(CommandTests, DisabledCommandDoesNotRun)
{
    int save = 0;
    CommandRegistry::Get().Register(CommandId::Save, { .key = "Save", .defaultShortcut = { ImGuiKey_S, ImGuiMod_Ctrl }, .isEnabled = [] { return false; }, .execute = [&save] { ++save; } });
    CommandRegistry::Get().BeginFrame();
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Global, Pressing({ { ImGuiKey_S, ImGuiMod_Ctrl } }));
    EXPECT_EQ(save, 0);
}

TEST_F(CommandTests, ReboundShortcutReplacesDefault)
{
    int save = 0;
    RegisterCounter(CommandId::Save, "Save", CommandScope::Global, { ImGuiKey_S, ImGuiMod_Ctrl }, save);
    CommandRegistry::Get().SetShortcut(CommandId::Save, { ImGuiKey_K, ImGuiMod_Ctrl });
    EXPECT_TRUE(CommandRegistry::Get().IsShortcutRebound(CommandId::Save));

    CommandRegistry::Get().BeginFrame();
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Global, Pressing({ { ImGuiKey_S, ImGuiMod_Ctrl } }));
    EXPECT_EQ(save, 0);
    CommandRegistry::Get().DispatchShortcuts(CommandScope::Global, Pressing({ { ImGuiKey_K, ImGuiMod_Ctrl } }));
    EXPECT_EQ(save, 1);

    CommandRegistry::Get().ResetShortcut(CommandId::Save);
    EXPECT_FALSE(CommandRegistry::Get().IsShortcutRebound(CommandId::Save));
}

TEST(ShortcutTextTests, FormatsSymbolsAndWords)
{
    KeyChord redo{ ImGuiKey_Z, ImGuiMod_Ctrl | ImGuiMod_Shift };
    EXPECT_EQ(Editor::ShortcutText(redo, Editor::ShortcutStyle::Symbols), "\xe2\x87\xa7\xe2\x8c\x98Z");
    EXPECT_EQ(Editor::ShortcutText(redo, Editor::ShortcutStyle::Words), "Ctrl+Shift+Z");

    KeyChord preferences{ ImGuiKey_Comma, ImGuiMod_Ctrl };
    EXPECT_EQ(Editor::ShortcutText(preferences, Editor::ShortcutStyle::Symbols), "\xe2\x8c\x98,");
    EXPECT_EQ(Editor::ShortcutText(preferences, Editor::ShortcutStyle::Words), "Ctrl+,");

    EXPECT_EQ(Editor::ShortcutText({ ImGuiKey_F2 }, Editor::ShortcutStyle::Words), "F2");
    EXPECT_EQ(Editor::ShortcutText({ ImGuiKey_Delete }, Editor::ShortcutStyle::Words), "Del");
    EXPECT_EQ(Editor::ShortcutText({}, Editor::ShortcutStyle::Words), "");
}

TEST_F(CommandTests, ConflictsAreFoundWithinScopeAndAgainstGlobal)
{
    int counter = 0;
    RegisterCounter(CommandId::Save, "Save", CommandScope::Global, { ImGuiKey_F }, counter);
    RegisterCounter(CommandId::FocusSelection, "FocusSelection", CommandScope::LevelView, { ImGuiKey_F }, counter);
    RegisterCounter(CommandId::GizmoMove, "GizmoMove", CommandScope::LevelView, { ImGuiKey_W }, counter);
    RegisterCounter(CommandId::Rename, "Rename", CommandScope::LevelContents, { ImGuiKey_W }, counter);

    EXPECT_EQ(CommandRegistry::Get().FindConflicts(CommandId::FocusSelection), std::vector<CommandId>{ CommandId::Save });
    EXPECT_EQ(CommandRegistry::Get().FindConflicts(CommandId::Save), std::vector<CommandId>{ CommandId::FocusSelection });
    EXPECT_TRUE(CommandRegistry::Get().FindConflicts(CommandId::GizmoMove).empty());
    EXPECT_EQ(CommandRegistry::Get().FindConflicts(CommandId::GizmoMove, { ImGuiKey_F }), std::vector<CommandId>({ CommandId::Save, CommandId::FocusSelection }));
    EXPECT_TRUE(CommandRegistry::Get().FindConflicts(CommandId::GizmoMove, {}).empty());
}

TEST_F(CommandTests, ReboundShortcutSurvivesPreferencesSaveAndLoad)
{
    int counter = 0;
    RegisterCounter(CommandId::Save, "Save", CommandScope::Global, { ImGuiKey_S, ImGuiMod_Ctrl }, counter);
    RegisterCounter(CommandId::Undo, "Undo", CommandScope::Global, { ImGuiKey_Z, ImGuiMod_Ctrl }, counter);
    CommandRegistry::Get().SetShortcut(CommandId::Save, { ImGuiKey_K, ImGuiMod_Ctrl | ImGuiMod_Shift });
    CommandRegistry::Get().SetShortcut(CommandId::Undo, {});

    std::filesystem::path path = Tests::MakeTempDirectory("Shortcuts") / "Preferences.json";
    Editor::EditorPreferences::Get().Load(path);
    Editor::EditorPreferences::Get().Save();

    CommandRegistry::Get().ResetShortcut(CommandId::Save);
    CommandRegistry::Get().ResetShortcut(CommandId::Undo);
    Editor::EditorPreferences::Get().Load(path);

    EXPECT_EQ(CommandRegistry::Get().GetShortcut(CommandId::Save), (KeyChord{ ImGuiKey_K, ImGuiMod_Ctrl | ImGuiMod_Shift }));
    EXPECT_FALSE(CommandRegistry::Get().GetShortcut(CommandId::Undo).IsBound());
}

TEST_F(CommandTests, InvalidShortcutEntriesKeepDefaults)
{
    int counter = 0;
    KeyChord save{ ImGuiKey_S, ImGuiMod_Ctrl };
    KeyChord undo{ ImGuiKey_Z, ImGuiMod_Ctrl };
    KeyChord redo{ ImGuiKey_Z, ImGuiMod_Ctrl | ImGuiMod_Shift };
    RegisterCounter(CommandId::Save, "Save", CommandScope::Global, save, counter);
    RegisterCounter(CommandId::Undo, "Undo", CommandScope::Global, undo, counter);
    RegisterCounter(CommandId::Redo, "Redo", CommandScope::Global, redo, counter);
    RegisterCounter(CommandId::Rename, "Rename", CommandScope::LevelContents, { ImGuiKey_F2 }, counter);

    Serialization::JsonDocument document;
    document.Parse(R"({
        "NoSuchCommand": { "key": "K", "modifiers": [] },
        "Save": { "key": "NotAKey", "modifiers": ["Ctrl"] },
        "Redo": { "key": "K", "modifiers": ["Hyper"] },
        "Undo": { "key": "K", "modifiers": "Ctrl" },
        "Rename": 5
    })");
    ASSERT_FALSE(document.HasParseError());
    Editor::ReadShortcutBindings(CommandRegistry::Get(), document);

    EXPECT_EQ(CommandRegistry::Get().GetShortcut(CommandId::Save), save);
    EXPECT_EQ(CommandRegistry::Get().GetShortcut(CommandId::Undo), undo);
    EXPECT_EQ(CommandRegistry::Get().GetShortcut(CommandId::Redo), redo);
    EXPECT_EQ(CommandRegistry::Get().GetShortcut(CommandId::Rename), (KeyChord{ ImGuiKey_F2 }));
}

TEST_F(CommandTests, OnlyReboundShortcutsAreWritten)
{
    int counter = 0;
    RegisterCounter(CommandId::Save, "Save", CommandScope::Global, { ImGuiKey_S, ImGuiMod_Ctrl }, counter);
    CommandRegistry::Get().Register(CommandId::Undo, { .key = "Undo", .defaultShortcut = { ImGuiKey_Z, ImGuiMod_Ctrl }, .execute = [] {} });
    CommandRegistry::Get().SetShortcut(CommandId::Undo, { ImGuiKey_U, ImGuiMod_Alt });

    Serialization::JsonDocument document;
    Serialization::JsonValue bindings = Editor::WriteShortcutBindings(CommandRegistry::Get(), document.GetAllocator());
    ASSERT_TRUE(bindings.IsObject());
    EXPECT_EQ(bindings.MemberCount(), 1u);
    const Serialization::JsonValue* undo = Serialization::FindMember(bindings, "Undo");
    ASSERT_NE(undo, nullptr);
    EXPECT_EQ(Serialization::GetString(*undo, "key"), "U");
    ASSERT_TRUE((*undo)["modifiers"].IsArray());
    EXPECT_STREQ((*undo)["modifiers"][0].GetString(), "Alt");
}
