#include "Assets/AssetDatabase.h"
#include "EditorContext.h"
#include "HotReload.h"
#include "Level/LevelSerializer.h"
#include "PlaySession.h"
#include "Reflection/TypeRegistry.h"
#include "Rendering/BuiltinAssets.h"
#include "Serialization/ObjectSerializer.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace BrightBox3D;
using BrightBox3D::Editor::EditorContext;
using BrightBox3D::Level::LevelObjectInstance;

namespace
{
    class ProjectFixture : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_root = std::filesystem::temp_directory_path() / ("BrightEditorTest_" + Core::Guid::Generate().ToString());
            std::string error;
            ASSERT_TRUE(Runtime::Project::Create(m_root, "EditorTest", error)) << error;
            ASSERT_TRUE(m_context.OpenProject(m_root, error)) << error;
        }

        void TearDown() override
        {
            m_context.CloseProject();
            std::error_code error;
            std::filesystem::remove_all(m_root, error);
        }

        std::filesystem::path m_root;
        EditorContext m_context;
    };

    void WriteFile(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << text;
    }
}

TEST_F(ProjectFixture, NewProjectHasStartupLevelWithDefaults)
{
    ASSERT_NE(m_context.GetEditLevel(), nullptr);
    EXPECT_TRUE(m_context.GetProject().GetStartupLevel().IsValid());
    EXPECT_NE(m_context.GetEditLevel()->FindObjectByName("Directional Light"), nullptr);
    EXPECT_NE(m_context.GetEditLevel()->FindObjectByName("Ground"), nullptr);
    EXPECT_FALSE(m_context.IsDirty());
}

TEST_F(ProjectFixture, CreateDeleteUndoRedo)
{
    Level::Level* level = m_context.GetEditLevel();
    size_t initial = level->GetObjectCount();
    LevelObjectInstance* cube = m_context.CreateModelObject(Rendering::BuiltinAssets::Cube(), "Cube", { 1, 2, 3 });
    ASSERT_NE(cube, nullptr);
    Core::Guid id = cube->GetId();
    EXPECT_TRUE(m_context.IsDirty());
    EXPECT_EQ(level->GetObjectCount(), initial + 1);

    m_context.Undo();
    EXPECT_EQ(level->FindObject(id), nullptr);
    m_context.Redo();
    ASSERT_NE(level->FindObject(id), nullptr);
    EXPECT_NEAR(level->FindObject(id)->GetTransform().GetPosition().y, 2.0f, 1.0e-4f);

    m_context.Select(id);
    m_context.DeleteSelection();
    EXPECT_EQ(level->FindObject(id), nullptr);
    m_context.Undo();
    EXPECT_NE(level->FindObject(id), nullptr);
}

TEST_F(ProjectFixture, PropertyChangesAreUndoable)
{
    Level::Level* level = m_context.GetEditLevel();
    LevelObjectInstance* object = m_context.CreateObject(LevelObjectInstance::StaticType(), "Thing");
    Core::Guid id = object->GetId();
    Serialization::JsonDocument before;
    EditorContext::Snapshot(*object, before);
    object->GetTransform().SetLocalPosition({ 5, 0, 0 });
    object->SetName("Renamed");
    m_context.RecordObjectChange(id, before, "Edit");
    m_context.Undo();
    ASSERT_NE(level->FindObject(id), nullptr);
    EXPECT_EQ(level->FindObject(id)->GetName(), "Thing");
    EXPECT_NEAR(level->FindObject(id)->GetTransform().GetPosition().x, 0.0f, 1.0e-4f);
    m_context.Redo();
    EXPECT_EQ(level->FindObject(id)->GetName(), "Renamed");
}

TEST_F(ProjectFixture, ReparentAndDuplicate)
{
    Level::Level* level = m_context.GetEditLevel();
    LevelObjectInstance* parent = m_context.CreateObject(LevelObjectInstance::StaticType(), "Parent");
    LevelObjectInstance* child = m_context.CreateObject(LevelObjectInstance::StaticType(), "Child");
    Core::Guid childId = child->GetId();
    m_context.Reparent(*child, parent, nullptr, -1);
    EXPECT_EQ(level->FindObject(childId)->GetParent(), parent);
    m_context.Undo();
    EXPECT_EQ(level->FindObject(childId)->GetParent(), nullptr);
    m_context.Redo();
    EXPECT_EQ(level->FindObject(childId)->GetParent(), parent);

    m_context.Select(parent->GetId());
    size_t count = level->GetObjectCount();
    m_context.DuplicateSelection();
    EXPECT_EQ(level->GetObjectCount(), count + 2);
    LevelObjectInstance* copy = m_context.GetPrimarySelection();
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy->GetId(), parent->GetId());
    EXPECT_EQ(copy->GetChildren().size(), 1u);
}

TEST_F(ProjectFixture, SaveAndReloadLevel)
{
    LevelObjectInstance* cube = m_context.CreateModelObject(Rendering::BuiltinAssets::Cube(), "SavedCube", { 0, 1, 0 });
    Core::Guid id = cube->GetId();
    ASSERT_TRUE(m_context.SaveLevel());
    EXPECT_FALSE(m_context.IsDirty());
    Core::Guid levelAsset = m_context.GetEditLevel()->GetAsset();
    ASSERT_TRUE(m_context.OpenLevel(levelAsset));
    ASSERT_NE(m_context.GetEditLevel()->FindObject(id), nullptr);
    EXPECT_EQ(m_context.GetEditLevel()->FindObject(id)->GetName(), "SavedCube");
}

TEST_F(ProjectFixture, LevelObjectAssetsLinkAndEditInIsolation)
{
    LevelObjectInstance* cube = m_context.CreateModelObject(Rendering::BuiltinAssets::Cube(), "Crate", { 0, 0, 0 });
    Core::Guid id = cube->GetId();
    Core::Guid asset = m_context.CreateLevelObjectAsset(*cube, "");
    ASSERT_TRUE(asset.IsValid());
    LevelObjectInstance* linked = m_context.GetEditLevel()->FindObject(id);
    ASSERT_NE(linked, nullptr);
    EXPECT_TRUE(linked->GetLink().isRoot);
    LevelObjectInstance* second = m_context.InstantiateLevelObject(asset, { 3, 0, 0 });
    ASSERT_NE(second, nullptr);
    Core::Guid secondId = second->GetId();

    ASSERT_TRUE(m_context.OpenLevelObjectEditor(asset));
    EXPECT_TRUE(m_context.IsEditingLevelObject());
    LevelObjectInstance* root = m_context.GetEditLevel()->GetCollections().front()->GetRoots().front();
    root->SetName("CrateRenamed");
    LevelObjectInstance* child = m_context.GetEditLevel()->CreateObject(LevelObjectInstance::StaticType(), nullptr, root);
    child->SetName("Lid");
    m_context.MarkDirty(*root);
    m_context.CloseLevelObjectEditor(true);
    EXPECT_FALSE(m_context.IsEditingLevelObject());

    LevelObjectInstance* refreshed = m_context.GetEditLevel()->FindObject(secondId);
    ASSERT_NE(refreshed, nullptr);
    ASSERT_EQ(refreshed->GetChildren().size(), 1u);
    EXPECT_EQ(refreshed->GetChildren()[0]->GetName(), "Lid");
}

TEST_F(ProjectFixture, PlaySessionRevertsUnlessKept)
{
    LevelObjectInstance* a = m_context.CreateObject(LevelObjectInstance::StaticType(), "A");
    LevelObjectInstance* b = m_context.CreateObject(LevelObjectInstance::StaticType(), "B");
    Core::Guid idA = a->GetId();
    Core::Guid idB = b->GetId();
    Editor::PlaySession session(m_context);
    m_context.SetPlaySession(&session);
    ASSERT_TRUE(session.Start());
    EXPECT_TRUE(m_context.IsPlaying());
    session.GetLevel()->FindObject(idA)->GetTransform().SetLocalPosition({ 9, 9, 9 });
    session.GetLevel()->FindObject(idB)->GetTransform().SetLocalPosition({ 7, 7, 7 });
    session.ToggleKeepChanges(idB);
    session.Tick(0.016f);
    session.Stop();
    m_context.SetPlaySession(nullptr);
    EXPECT_NEAR(m_context.GetEditLevel()->FindObject(idA)->GetTransform().GetPosition().x, 0.0f, 1.0e-4f);
    EXPECT_NEAR(m_context.GetEditLevel()->FindObject(idB)->GetTransform().GetPosition().x, 7.0f, 1.0e-4f);
}

TEST_F(ProjectFixture, HotReloadCompilesAndPreservesState)
{
    if (Editor::HotReload::FindCMake().empty())
    {
        GTEST_SKIP() << "CMake is not available";
    }
    std::filesystem::path source = m_root / "Source";
    WriteFile(source / "Scorer.h", R"(#pragma once

#include "Level/LevelObjectInstance.h"

BrightClass()
class Scorer : public BrightBox3D::Level::LevelObjectInstance
{
    BrightBody()
public:
    void OnGameUpdate(float deltaTime) override;

    BrightProperty(EditAnywhere)
    int m_score = 0;
};
)");
    WriteFile(source / "Scorer.cpp", R"(#include "Scorer.h"

void Scorer::OnGameUpdate(float deltaTime)
{
    m_score += 1;
}
)");

    Editor::HotReload reload(m_context);
    auto compile = [&]()
    {
        reload.RequestCompile();
        auto start = std::chrono::steady_clock::now();
        do
        {
            reload.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } while ((reload.IsCompiling() || reload.GetState() == Editor::CompileState::Idle) && std::chrono::steady_clock::now() - start < std::chrono::minutes(5));
        return reload.GetState() == Editor::CompileState::Succeeded;
    };
    ASSERT_TRUE(compile()) << reload.GetStatusText();
    const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find("Scorer");
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->GetModuleName(), m_context.GetProject().GetGameModuleName());

    Level::Level* level = m_context.GetEditLevel();
    LevelObjectInstance* parent = level->CreateObject(LevelObjectInstance::StaticType());
    LevelObjectInstance* scorer = level->CreateObject(*type);
    Core::Guid id = scorer->GetId();
    LevelObjectInstance* child = level->CreateObject(LevelObjectInstance::StaticType(), nullptr, scorer);
    Core::Guid childId = child->GetId();
    scorer->SetParent(parent);
    scorer->GetTransform().SetLocalPosition({ 4, 5, 6 });
    type->FindProperty("m_score")->GetValue<int>(scorer) = 41;

    WriteFile(source / "Scorer.h", R"(#pragma once

#include "Level/LevelObjectInstance.h"

BrightClass()
class Scorer : public BrightBox3D::Level::LevelObjectInstance
{
    BrightBody()
public:
    void OnGameUpdate(float deltaTime) override;

    BrightProperty(EditAnywhere)
    int m_score = 0;

    BrightProperty(EditAnywhere)
    float m_bonus = 2.5f;
};
)");
    WriteFile(source / "Scorer.cpp", R"(#include "Scorer.h"

void Scorer::OnGameUpdate(float deltaTime)
{
    m_score += 10;
}
)");
    ASSERT_TRUE(compile()) << reload.GetStatusText();
    EXPECT_EQ(reload.GetModule().GetGeneration(), 2u);

    const Reflection::TypeInfo* reloaded = Reflection::TypeRegistry::Find("Scorer");
    ASSERT_NE(reloaded, nullptr);
    ASSERT_NE(reloaded->FindProperty("m_bonus"), nullptr);
    LevelObjectInstance* restored = level->FindObject(id);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(&restored->GetType(), reloaded);
    EXPECT_EQ(reloaded->FindProperty("m_score")->GetValue<int>(restored), 41);
    EXPECT_FLOAT_EQ(reloaded->FindProperty("m_bonus")->GetValue<float>(restored), 2.5f);
    EXPECT_EQ(restored->GetParent(), parent);
    EXPECT_NEAR(restored->GetTransform().GetLocalPosition().y, 5.0f, 1.0e-4f);
    ASSERT_EQ(restored->GetChildren().size(), 1u);
    EXPECT_EQ(restored->GetChildren()[0]->GetId(), childId);
    EXPECT_EQ(restored->GetChildren()[0]->GetParent(), restored);

    level->BeginPlay();
    level->Update(0.016f);
    level->Update(0.016f);
    EXPECT_EQ(reloaded->FindProperty("m_score")->GetValue<int>(restored), 61);
    level->EndPlay();

    reload.OnProjectClosed();
    EXPECT_EQ(Reflection::TypeRegistry::Find("Scorer"), nullptr);
    LevelObjectInstance* missing = level->FindObject(id);
    ASSERT_NE(missing, nullptr);
    EXPECT_EQ(missing->GetMissingTypeName(), "Scorer");
}
