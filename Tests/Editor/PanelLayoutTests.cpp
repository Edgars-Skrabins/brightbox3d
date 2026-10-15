#include "PanelLayoutFile.h"
#include "Panels/EditorPanel.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace BrightBox3D;
using Editor::EditorPanel;
using Editor::EditorWindow;

namespace
{
    constexpr const char* ValueKey = "value";

    class TestPanel : public EditorPanel
    {
    public:
        TestPanel(EditorWindow type, int index, std::string value) : EditorPanel(type, index), m_value(std::move(value)) {}

        void Draw() override {}

        void SaveState(Serialization::JsonValue& state, Serialization::JsonAllocator& allocator) const override
        {
            Serialization::SetMember(state, ValueKey, Serialization::MakeString(m_value, allocator), allocator);
        }

        void LoadState(const Serialization::JsonValue& state) override
        {
            m_value = Serialization::GetString(state, ValueKey);
        }

        const std::string& GetValue() const { return m_value; }

    private:
        std::string m_value;
    };

    std::vector<Editor::PanelLayoutFile::PanelEntry> ReadText(const std::string& text, Serialization::JsonDocument& document)
    {
        EXPECT_TRUE(Serialization::ParseJson(text, document));
        std::optional<std::vector<Editor::PanelLayoutFile::PanelEntry>> entries = Editor::PanelLayoutFile::Read(document);
        EXPECT_TRUE(entries.has_value());
        return entries.value_or(std::vector<Editor::PanelLayoutFile::PanelEntry>{});
    }
}

TEST(PanelLayoutTests, OpenPanelsRoundTripWithTypeIndexAndState)
{
    std::vector<std::unique_ptr<EditorPanel>> panels;
    panels.push_back(std::make_unique<TestPanel>(EditorWindow::LevelView, 1, "camera a"));
    panels.push_back(std::make_unique<TestPanel>(EditorWindow::LevelView, 2, "camera b"));
    panels.push_back(std::make_unique<TestPanel>(EditorWindow::Terminal, 1, "terminal"));
    panels.push_back(std::make_unique<TestPanel>(EditorWindow::Assets, 3, "Materials"));
    panels.push_back(std::make_unique<TestPanel>(EditorWindow::Inspector, 1, "closed"));
    panels.back()->Close();

    Serialization::JsonDocument written;
    Editor::PanelLayoutFile::Write(panels, written);
    Serialization::JsonDocument parsed;
    std::vector<Editor::PanelLayoutFile::PanelEntry> entries = ReadText(Serialization::WriteJson(written), parsed);

    ASSERT_EQ(entries.size(), 4u);
    const EditorWindow types[] = { EditorWindow::LevelView, EditorWindow::LevelView, EditorWindow::Terminal, EditorWindow::Assets };
    const int indexes[] = { 1, 2, 1, 3 };
    const char* values[] = { "camera a", "camera b", "terminal", "Materials" };
    for (size_t i = 0; i < entries.size(); ++i)
    {
        EXPECT_EQ(entries[i].type, types[i]);
        EXPECT_EQ(entries[i].index, indexes[i]);
        ASSERT_NE(entries[i].state, nullptr);
        TestPanel restored(entries[i].type, entries[i].index, "");
        restored.LoadState(*entries[i].state);
        EXPECT_EQ(restored.GetValue(), values[i]);
    }
}

TEST(PanelLayoutTests, InvalidEntriesAreSkippedAndInvalidFilesRejected)
{
    Serialization::JsonDocument document;
    std::vector<Editor::PanelLayoutFile::PanelEntry> entries = ReadText(R"([
        { "type": "Terminal", "index": 1 },
        { "type": "Terminal", "index": 1 },
        { "type": "Preferences", "index": 1 },
        { "type": "Nonsense", "index": 1 },
        { "type": "Assets", "index": 0 },
        { "type": "Assets", "index": "two" },
        { "type": "Inspector", "index": 2, "state": 5 },
        "broken"
    ])", document);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].type, EditorWindow::Terminal);
    EXPECT_EQ(entries[1].type, EditorWindow::Inspector);
    EXPECT_EQ(entries[1].index, 2);
    EXPECT_EQ(entries[1].state, nullptr);

    Serialization::JsonDocument object;
    ASSERT_TRUE(Serialization::ParseJson(R"({ "panels": [] })", object));
    EXPECT_FALSE(Editor::PanelLayoutFile::Read(object).has_value());

    Serialization::JsonDocument empty;
    ASSERT_TRUE(Serialization::ParseJson("[]", empty));
    ASSERT_TRUE(Editor::PanelLayoutFile::Read(empty).has_value());
    EXPECT_TRUE(Editor::PanelLayoutFile::Read(empty)->empty());
}
