#include "Assets/AssetDatabase.h"
#include "Common/TestUtilities.h"
#include "Core/Log.h"
#include "EditorContext.h"
#include "Framework/LevelGameplaySettings.h"
#include "HotReload.h"
#include "Level/LevelSerializer.h"
#include "Level/LevelTestTypes.h"
#include "PlaySession.h"
#include "Reflection/TypeRegistry.h"
#include "Rendering/BuiltinAssets.h"
#include "Scripts/PendingScriptComponent.h"
#include "Scripts/ScriptFiles.h"
#include "Serialization/ObjectSerializer.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace BrightBox3D;
using BrightBox3D::Editor::EditorContext;
using BrightBox3D::Level::LevelObject;

namespace
{
    class ProjectFixture : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_root = Tests::MakeTempDirectory("Editor");
            ASSERT_TRUE(Tests::Succeeded(Runtime::Project::Create(m_root, "EditorTest")));
            ASSERT_TRUE(Tests::Succeeded(m_context.GetProjectService().OpenProject(m_root)));
        }

        void TearDown() override
        {
            m_context.GetProjectService().CloseProject();
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
    ASSERT_NE(m_context.GetLevelService().GetEditLevel(), nullptr);
    EXPECT_TRUE(m_context.GetProjectService().GetProject().GetStartupLevel().IsValid());
    EXPECT_NE(m_context.GetLevelService().GetEditLevel()->FindObjectByName("Directional Light"), nullptr);
    EXPECT_NE(m_context.GetLevelService().GetEditLevel()->FindObjectByName("Ground"), nullptr);
    EXPECT_FALSE(m_context.GetLevelService().IsDirty());
}

TEST_F(ProjectFixture, CreateDeleteUndoRedo)
{
    Level::Level* level = m_context.GetLevelService().GetEditLevel();
    size_t initial = level->GetObjectCount();
    LevelObject* cube = m_context.GetObjectEditService().CreateModelObject(Rendering::BuiltinAssets::Cube(), "Cube", { 1, 2, 3 });
    ASSERT_NE(cube, nullptr);
    Core::Guid id = cube->GetId();
    EXPECT_TRUE(m_context.GetLevelService().IsDirty());
    EXPECT_EQ(level->GetObjectCount(), initial + 1);

    m_context.GetUndoService().Undo();
    EXPECT_EQ(level->FindObject(id), nullptr);
    m_context.GetUndoService().Redo();
    ASSERT_NE(level->FindObject(id), nullptr);
    EXPECT_NEAR(level->FindObject(id)->GetTransform().GetPosition().y, 2.0f, 1.0e-4f);

    m_context.GetSelectionService().Select(id);
    m_context.GetObjectEditService().DeleteSelection();
    EXPECT_EQ(level->FindObject(id), nullptr);
    m_context.GetUndoService().Undo();
    EXPECT_NE(level->FindObject(id), nullptr);
}

TEST_F(ProjectFixture, PropertyChangesAreUndoable)
{
    Level::Level* level = m_context.GetLevelService().GetEditLevel();
    LevelObject* object = m_context.GetObjectEditService().CreateObject(LevelObject::StaticType(), "Thing");
    Core::Guid id = object->GetId();
    Serialization::JsonDocument before;
    Editor::ObjectEditService::Snapshot(*object, before);
    object->GetTransform().SetLocalPosition({ 5, 0, 0 });
    object->SetName("Renamed");
    m_context.GetUndoService().RecordObjectChange(id, before, "Edit");
    m_context.GetUndoService().Undo();
    ASSERT_NE(level->FindObject(id), nullptr);
    EXPECT_EQ(level->FindObject(id)->GetName(), "Thing");
    EXPECT_NEAR(level->FindObject(id)->GetTransform().GetPosition().x, 0.0f, 1.0e-4f);
    m_context.GetUndoService().Redo();
    EXPECT_EQ(level->FindObject(id)->GetName(), "Renamed");
}

TEST_F(ProjectFixture, ReparentAndDuplicate)
{
    Level::Level* level = m_context.GetLevelService().GetEditLevel();
    LevelObject* parent = m_context.GetObjectEditService().CreateObject(LevelObject::StaticType(), "Parent");
    LevelObject* child = m_context.GetObjectEditService().CreateObject(LevelObject::StaticType(), "Child");
    Core::Guid childId = child->GetId();
    m_context.GetObjectEditService().Reparent(*child, parent, nullptr, std::nullopt);
    EXPECT_EQ(level->FindObject(childId)->GetParent(), parent);
    m_context.GetUndoService().Undo();
    EXPECT_EQ(level->FindObject(childId)->GetParent(), nullptr);
    m_context.GetUndoService().Redo();
    EXPECT_EQ(level->FindObject(childId)->GetParent(), parent);

    m_context.GetSelectionService().Select(parent->GetId());
    size_t count = level->GetObjectCount();
    m_context.GetObjectEditService().DuplicateSelection();
    EXPECT_EQ(level->GetObjectCount(), count + 2);
    LevelObject* copy = m_context.GetSelectionService().GetPrimarySelection();
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy->GetId(), parent->GetId());
    EXPECT_EQ(copy->GetChildren().size(), 1u);
}

TEST_F(ProjectFixture, SaveAndReloadLevel)
{
    LevelObject* cube = m_context.GetObjectEditService().CreateModelObject(Rendering::BuiltinAssets::Cube(), "SavedCube", { 0, 1, 0 });
    Core::Guid id = cube->GetId();
    ASSERT_TRUE(m_context.GetLevelService().SaveLevel());
    EXPECT_FALSE(m_context.GetLevelService().IsDirty());
    Core::Guid levelAsset = m_context.GetLevelService().GetEditLevel()->GetAsset();
    ASSERT_TRUE(m_context.GetLevelService().OpenLevel(levelAsset));
    ASSERT_NE(m_context.GetLevelService().GetEditLevel()->FindObject(id), nullptr);
    EXPECT_EQ(m_context.GetLevelService().GetEditLevel()->FindObject(id)->GetName(), "SavedCube");
}

TEST_F(ProjectFixture, LevelObjectAssetsLinkAndEditInIsolation)
{
    LevelObject* cube = m_context.GetObjectEditService().CreateModelObject(Rendering::BuiltinAssets::Cube(), "Crate", { 0, 0, 0 });
    Core::Guid id = cube->GetId();
    Core::Guid asset = m_context.GetAssetCreationService().CreateVariantAsset(*cube, "");
    ASSERT_TRUE(asset.IsValid());
    LevelObject* linked = m_context.GetLevelService().GetEditLevel()->FindObject(id);
    ASSERT_NE(linked, nullptr);
    EXPECT_TRUE(linked->GetLink().isRoot);
    LevelObject* second = m_context.GetObjectEditService().InstantiateLevelObject(asset, { 3, 0, 0 });
    ASSERT_NE(second, nullptr);
    Core::Guid secondId = second->GetId();

    ASSERT_TRUE(m_context.GetLevelObjectEditService().OpenLevelObjectEditor(asset));
    EXPECT_TRUE(m_context.GetLevelObjectEditService().IsEditingLevelObject());
    LevelObject* root = m_context.GetLevelService().GetEditLevel()->GetCollections().front()->GetRoots().front();
    root->SetName("CrateRenamed");
    LevelObject* child = *m_context.GetLevelService().GetEditLevel()->CreateObject(LevelObject::StaticType(), nullptr, root);
    child->SetName("Lid");
    m_context.GetLevelService().MarkDirty(*root);
    m_context.GetLevelObjectEditService().CloseLevelObjectEditor(Editor::EditorChanges::Save);
    EXPECT_FALSE(m_context.GetLevelObjectEditService().IsEditingLevelObject());

    LevelObject* refreshed = m_context.GetLevelService().GetEditLevel()->FindObject(secondId);
    ASSERT_NE(refreshed, nullptr);
    ASSERT_EQ(refreshed->GetChildren().size(), 1u);
    EXPECT_EQ(refreshed->GetChildren()[0]->GetName(), "Lid");
}

TEST_F(ProjectFixture, PlaySessionRevertsUnlessKept)
{
    LevelObject* a = m_context.GetObjectEditService().CreateObject(LevelObject::StaticType(), "A");
    LevelObject* b = m_context.GetObjectEditService().CreateObject(LevelObject::StaticType(), "B");
    Core::Guid idA = a->GetId();
    Core::Guid idB = b->GetId();
    Editor::PlaySession session(m_context);
    m_context.GetLevelService().SetPlaySession(&session);
    ASSERT_TRUE(session.Start());
    EXPECT_TRUE(m_context.GetLevelService().IsPlaying());
    session.GetLevel()->FindObject(idA)->GetTransform().SetLocalPosition({ 9, 9, 9 });
    session.GetLevel()->FindObject(idB)->GetTransform().SetLocalPosition({ 7, 7, 7 });
    session.ToggleKeepChanges(idB);
    session.Tick(0.016f);
    session.Stop();
    m_context.GetLevelService().SetPlaySession(nullptr);
    EXPECT_NEAR(m_context.GetLevelService().GetEditLevel()->FindObject(idA)->GetTransform().GetPosition().x, 0.0f, 1.0e-4f);
    EXPECT_NEAR(m_context.GetLevelService().GetEditLevel()->FindObject(idB)->GetTransform().GetPosition().x, 7.0f, 1.0e-4f);
}

TEST_F(ProjectFixture, EveryScriptTemplateCompilesAndRegisters)
{
    if (Editor::HotReload::FindCMake().empty())
    {
        GTEST_SKIP() << "CMake is not available";
    }
    std::filesystem::path scripts = m_context.GetProjectService().GetProject().GetScriptsDirectory();
    for (size_t index = 0; index < static_cast<size_t>(Editor::ScriptTemplate::Count); ++index)
    {
        Editor::ScriptTemplate scriptTemplate = static_cast<Editor::ScriptTemplate>(index);
        std::string className = std::string("Template") + Editor::GetScriptTemplateInfo(scriptTemplate).fileStem;
        ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::ValidateNewClassName(className, m_context.GetScriptLibrary())));
        ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::CreateScript(scripts, "Templates", className, scriptTemplate)));
    }
    EXPECT_FALSE(Editor::ScriptFiles::ValidateNewClassName("2Fast", m_context.GetScriptLibrary()));
    EXPECT_FALSE(Editor::ScriptFiles::ValidateNewClassName("LevelObject", m_context.GetScriptLibrary()));
    m_context.GetScriptLibrary().Refresh();
    EXPECT_FALSE(Editor::ScriptFiles::ValidateNewClassName("TemplateComponent", m_context.GetScriptLibrary()));

    Editor::HotReload reload(m_context);
    reload.RequestCompile();
    auto start = std::chrono::steady_clock::now();
    do
    {
        reload.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while ((reload.IsCompiling() || reload.GetState() == Editor::CompileState::Idle) && std::chrono::steady_clock::now() - start < std::chrono::minutes(5));
    ASSERT_EQ(reload.GetState(), Editor::CompileState::Succeeded) << reload.GetStatusText();
    for (size_t index = 0; index < static_cast<size_t>(Editor::ScriptTemplate::Count); ++index)
    {
        const Editor::ScriptTemplateInfo& info = Editor::GetScriptTemplateInfo(static_cast<Editor::ScriptTemplate>(index));
        const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(std::string("Template") + info.fileStem);
        ASSERT_NE(type, nullptr) << info.fileStem;
        EXPECT_EQ(type->GetBase(), &info.baseType()) << info.fileStem;
        EXPECT_EQ(type->GetHeaderPath(), std::string("Templates/Template") + info.fileStem + ".h");
    }
}

TEST_F(ProjectFixture, ProjectsKeepScriptsApartFromAssets)
{
    Runtime::Project& project = m_context.GetProjectService().GetProject();
    EXPECT_EQ(project.GetScriptsDirectory(), project.GetRoot() / "Scripts");
    EXPECT_TRUE(std::filesystem::is_directory(project.GetScriptsDirectory()));
    EXPECT_TRUE(std::filesystem::is_directory(project.GetAssetsDirectory()));
    EXPECT_FALSE(std::filesystem::exists(project.GetRoot() / "Source"));
    EXPECT_EQ(m_context.GetScriptLibrary().GetRoot(), project.GetScriptsDirectory());
    m_context.GetProjectService().CloseProject();
    EXPECT_TRUE(m_context.GetScriptLibrary().GetRoot().empty());
}

TEST_F(ProjectFixture, PendingScriptComponentWaitsForItsClassAndTarget)
{
    Level::Level* level = m_context.GetLevelService().GetEditLevel();
    LevelObject* object = *level->CreateObject(LevelObject::StaticType());
    Core::Guid id = object->GetId();
    const Reflection::TypeInfo& recording = Tests::RecordingComponent::StaticType();

    Editor::PendingScriptComponent pending;
    pending.Request(id, "NotCompiledYet");
    pending.OnReloaded(m_context);
    EXPECT_TRUE(pending.IsPending());
    pending.Clear();
    EXPECT_FALSE(pending.IsPending());

    pending.Request(id, LevelObject::StaticType().GetName());
    pending.OnReloaded(m_context);
    EXPECT_TRUE(pending.IsPending());
    EXPECT_EQ(object->GetComponents().size(), 0u);

    pending.Request(id, recording.GetName());
    pending.OnReloaded(m_context);
    EXPECT_FALSE(pending.IsPending());
    EXPECT_NE(object->GetComponent(recording), nullptr);

    pending.Request(id, recording.GetName());
    level->DestroyObjectImmediate(object);
    pending.OnReloaded(m_context);
    EXPECT_FALSE(pending.IsPending());
}

TEST_F(ProjectFixture, ScriptCreatedFromAddComponentIsAddedAfterCompiling)
{
    if (Editor::HotReload::FindCMake().empty())
    {
        GTEST_SKIP() << "CMake is not available";
    }
    Level::Level* level = m_context.GetLevelService().GetEditLevel();
    LevelObject* object = *level->CreateObject(LevelObject::StaticType());
    Core::Guid id = object->GetId();
    std::filesystem::path scripts = m_context.GetProjectService().GetProject().GetScriptsDirectory();
    ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::CreateScript(scripts, "", "Spinner", Editor::ScriptTemplate::Component)));

    Editor::PendingScriptComponent pending;
    pending.Request(id, "Spinner");
    Editor::HotReload reload(m_context);
    reload.OnReloaded = [&] { pending.OnReloaded(m_context); };
    reload.OnCompileFailed = [&] { pending.Clear(); };
    reload.RequestCompile();
    auto start = std::chrono::steady_clock::now();
    do
    {
        reload.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while ((reload.IsCompiling() || reload.GetState() == Editor::CompileState::Idle) && std::chrono::steady_clock::now() - start < std::chrono::minutes(5));
    ASSERT_EQ(reload.GetState(), Editor::CompileState::Succeeded) << reload.GetStatusText();

    EXPECT_FALSE(pending.IsPending());
    const Reflection::TypeInfo* spinner = Reflection::TypeRegistry::Find("Spinner");
    ASSERT_NE(spinner, nullptr);
    LevelObject* current = m_context.GetLevelService().GetEditLevel()->FindObject(id);
    ASSERT_NE(current, nullptr);
    EXPECT_NE(current->GetComponent(*spinner), nullptr);
    m_context.GetUndoService().Undo();
    current = m_context.GetLevelService().GetEditLevel()->FindObject(id);
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->GetComponent(*spinner), nullptr);
}

TEST_F(ProjectFixture, RenamedScriptsKeepSavedObjectsLoading)
{
    if (Editor::HotReload::FindCMake().empty())
    {
        GTEST_SKIP() << "CMake is not available";
    }
    std::filesystem::path scripts = m_context.GetProjectService().GetProject().GetScriptsDirectory();
    ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::CreateScript(scripts, "", "Wanderer", Editor::ScriptTemplate::LevelObject)));
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
    Level::Level* level = m_context.GetLevelService().GetEditLevel();
    LevelObject* wanderer = *level->CreateObject(*Reflection::TypeRegistry::Find("Wanderer"));
    Serialization::JsonDocument saved;
    Editor::ObjectEditService::Snapshot(*wanderer, saved);
    level->DestroyObjectImmediate(wanderer);

    Editor::ScriptLibrary& library = m_context.GetScriptLibrary();
    library.Refresh();
    ASSERT_TRUE(Tests::Succeeded(Editor::ScriptFiles::RenameScript(*library.FindByClass("Wanderer"), "Drifter", library)));
    ASSERT_TRUE(compile()) << reload.GetStatusText();
    const Reflection::TypeInfo* drifter = Reflection::TypeRegistry::Find("Drifter");
    ASSERT_NE(drifter, nullptr);
    EXPECT_EQ(Reflection::TypeRegistry::Find("Wanderer"), drifter);
    LevelObject* restored = Level::LevelSerializer::ReadObject(*level, nullptr, nullptr, saved, Level::Initialization::Immediate);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(&restored->GetType(), drifter);
    level->DestroyObjectImmediate(restored);
}

TEST_F(ProjectFixture, HotReloadCompilesAndPreservesState)
{
    if (Editor::HotReload::FindCMake().empty())
    {
        GTEST_SKIP() << "CMake is not available";
    }
    std::filesystem::path source = m_root / "Scripts";
    WriteFile(source / "Scorer.h", R"(#pragma once

#include "Level/LevelObject.h"

BrightClass()
class Scorer : public BrightBox3D::Level::LevelObject
{
    BrightBody()
public:
    void OnGameUpdate(float deltaTime) override;

    BrightProperty(.edit = Edit::Anywhere)
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
    EXPECT_EQ(type->GetModuleName(), m_context.GetProjectService().GetProject().GetGameModuleName());

    Level::Level* level = m_context.GetLevelService().GetEditLevel();
    LevelObject* parent = *level->CreateObject(LevelObject::StaticType());
    LevelObject* scorer = *level->CreateObject(*type);
    Core::Guid id = scorer->GetId();
    LevelObject* child = *level->CreateObject(LevelObject::StaticType(), nullptr, scorer);
    Core::Guid childId = child->GetId();
    scorer->SetParent(parent);
    scorer->GetTransform().SetLocalPosition({ 4, 5, 6 });
    type->FindProperty("m_score")->GetValue<int>(scorer) = 41;

    WriteFile(source / "Scorer.h", R"(#pragma once

#include "Level/LevelObject.h"

BrightClass()
class Scorer : public BrightBox3D::Level::LevelObject
{
    BrightBody()
public:
    void OnGameUpdate(float deltaTime) override;

    BrightProperty(.edit = Edit::Anywhere)
    int m_score = 0;

    BrightProperty(.edit = Edit::Anywhere)
    float m_bonus = 2.5f;
};
)");
    WriteFile(source / "Scorer.cpp", R"(#include "Scorer.h"

#include "Core/Debug.h"

void Scorer::OnGameUpdate(float deltaTime)
{
    m_score += 10;
    BrightBox3D::Debug::Log("Scorer updated");
}
)");
    ASSERT_TRUE(compile()) << reload.GetStatusText();
    EXPECT_EQ(reload.GetModule().GetGeneration(), 2u);

    const Reflection::TypeInfo* reloaded = Reflection::TypeRegistry::Find("Scorer");
    ASSERT_NE(reloaded, nullptr);
    ASSERT_NE(reloaded->FindProperty("m_bonus"), nullptr);
    LevelObject* restored = level->FindObject(id);
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
    int gameLogs = 0;
    Core::Log::ForEach([&](const Core::LogEntry& entry)
    {
        if (entry.message == "Scorer updated")
        {
            EXPECT_EQ(entry.source, Core::LogSource::Game);
            EXPECT_NE(entry.file.find("Scorer.cpp"), std::string::npos) << entry.file;
            ++gameLogs;
        }
    });
    EXPECT_EQ(gameLogs, 2);

    reload.OnProjectClosed();
    EXPECT_EQ(Reflection::TypeRegistry::Find("Scorer"), nullptr);
    LevelObject* missing = level->FindObject(id);
    ASSERT_NE(missing, nullptr);
    EXPECT_EQ(missing->GetMissingTypeName(), "Scorer");
}

TEST_F(ProjectFixture, CancellingACompileStopsQuicklyAndKeepsTheModule)
{
    std::filesystem::path source = m_context.GetProjectService().GetProject().GetScriptsDirectory();
    WriteFile(source / "Counter.h", R"(#pragma once

#include "Level/LevelObject.h"

BrightClass()
class Counter : public BrightBox3D::Level::LevelObject
{
    BrightBody()
public:
    BrightProperty(.edit = Edit::Anywhere)
    int m_count = 0;
};
)");
    WriteFile(source / "Counter.cpp", "#include \"Counter.h\"\n");

    Editor::HotReload reload(m_context);
    reload.RequestCompile();
    auto start = std::chrono::steady_clock::now();
    do
    {
        reload.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while ((reload.IsCompiling() || reload.GetState() == Editor::CompileState::Idle) && std::chrono::steady_clock::now() - start < std::chrono::minutes(5));
    ASSERT_EQ(reload.GetState(), Editor::CompileState::Succeeded) << reload.GetStatusText();
    std::filesystem::path module = reload.GetModulePath();
    ASSERT_TRUE(std::filesystem::exists(module));
    auto moduleTime = std::filesystem::last_write_time(module);

    WriteFile(source / "Counter.cpp", "#include \"Counter.h\"\n\nint CounterValue() { return 1; }\n");
    reload.RequestCompile();
    reload.Update();
    ASSERT_TRUE(reload.IsCompiling());

    auto cancelStart = std::chrono::steady_clock::now();
    reload.Cancel();
    EXPECT_LT(std::chrono::steady_clock::now() - cancelStart, std::chrono::seconds(3));
    EXPECT_EQ(reload.GetState(), Editor::CompileState::Idle);
    EXPECT_EQ(reload.GetStatusText(), "Compile cancelled");
    EXPECT_EQ(std::filesystem::last_write_time(module), moduleTime);
    EXPECT_TRUE(reload.GetModule().IsLoaded());
    reload.OnProjectClosed();
}

TEST_F(ProjectFixture, FailedReloadRestoresThePreviousModuleAndSaysSo)
{
    std::filesystem::path source = m_context.GetProjectService().GetProject().GetScriptsDirectory();
    WriteFile(source / "Keeper.h", R"(#pragma once

#include "Level/LevelObject.h"

BrightClass()
class Keeper : public BrightBox3D::Level::LevelObject
{
    BrightBody()
public:
    BrightProperty(.edit = Edit::Anywhere)
    int m_value = 0;
};
)");
    WriteFile(source / "Keeper.cpp", "#include \"Keeper.h\"\n");

    Editor::HotReload reload(m_context);
    reload.RequestCompile();
    auto start = std::chrono::steady_clock::now();
    do
    {
        reload.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while ((reload.IsCompiling() || reload.GetState() == Editor::CompileState::Idle) && std::chrono::steady_clock::now() - start < std::chrono::minutes(5));
    ASSERT_EQ(reload.GetState(), Editor::CompileState::Succeeded) << reload.GetStatusText();
    Runtime::GameModule& module = reload.GetModule();
    std::filesystem::path previous = module.GetLoadedCopy();
    ASSERT_FALSE(previous.empty());

    std::filesystem::path corrupt = m_root / "Binaries" / ("Corrupt" + reload.GetModulePath().extension().string());
    WriteFile(corrupt, "not a library");
    Core::Log::Clear();
    Core::Result<void> reloaded = module.Reload(corrupt);
    ASSERT_FALSE(reloaded);
    EXPECT_FALSE(reloaded.GetError().GetText().empty());

    bool reportedRestore = false;
    Core::Log::ForEach([&](const Core::LogEntry& entry)
    {
        reportedRestore |= entry.message.find("the previous module was restored") != std::string::npos;
    });
    EXPECT_TRUE(reportedRestore);
    EXPECT_TRUE(module.IsLoaded());
    EXPECT_EQ(module.GetLoadedCopy(), previous);
    EXPECT_TRUE(std::filesystem::exists(module.GetLoadedCopy()));
    EXPECT_NE(Reflection::TypeRegistry::Find("Keeper"), nullptr);
    reload.OnProjectClosed();
}

TEST_F(ProjectFixture, LegacyProjectGameModeMovesIntoLevels)
{
    m_context.GetProjectService().CloseProject();
    std::filesystem::path projectFile = m_root / Runtime::Project::FileName;
    Serialization::JsonDocument project;
    ASSERT_TRUE(Serialization::LoadJsonFile(projectFile, project));
    Serialization::JsonValue legacy(rapidjson::kObjectType);
    legacy.AddMember("$type", "GameMode", project.GetAllocator());
    legacy.AddMember("m_playerCount", 3, project.GetAllocator());
    Serialization::SetMember(project, "gameMode", std::move(legacy), project.GetAllocator());
    ASSERT_TRUE(Serialization::SaveJsonFile(projectFile, project));

    ASSERT_TRUE(Tests::Succeeded(m_context.GetProjectService().OpenProject(m_root)));
    const Serialization::JsonValue* entry = Framework::LevelGameplaySettings::FindEntry(m_context.GetLevelService().GetEditLevel()->GetSettings(), Framework::GameplaySlot::GameMode);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(Framework::LevelGameplaySettings::GetClassName(*entry), "GameModeBase");
    const Serialization::JsonValue* settings = Framework::LevelGameplaySettings::FindObjectSettings(*entry);
    ASSERT_NE(settings, nullptr);
    EXPECT_EQ(Serialization::GetNumber(*settings, "m_playerCount", 0.0), 3.0);

    Serialization::JsonDocument saved;
    ASSERT_TRUE(Serialization::LoadJsonFile(projectFile, saved));
    EXPECT_FALSE(saved.HasMember("gameMode"));
}
