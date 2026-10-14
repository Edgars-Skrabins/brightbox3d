#include "Assets/AssetDatabase.h"
#include "Common/TestUtilities.h"
#include "Core/FileSystem.h"
#include "Core/Process.h"
#include "EditorContext.h"
#include "Framework/Camera.h"
#include "Framework/LevelGameplaySettings.h"
#include "Framework/PlayerStart.h"
#include "GameBuilder.h"
#include "HotReload.h"
#include "Level/LevelSerializer.h"
#include "Physics/Colliders.h"
#include "Physics/Rigidbody.h"
#include "Reflection/TypeRegistry.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MeshRenderer.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <thread>

using namespace BrightBox3D;
using BrightBox3D::Level::LevelObject;

namespace
{
    const std::filesystem::path SampleRoot = std::filesystem::path(BRIGHT_SAMPLES_DIR) / "ArenaDemo";
    const char* const CopiedFolders[] = { "Source", "Assets/UI", "Assets/Materials" };
    const char* const AuthoredFolders[] = { "Assets/Levels", "Assets/Prefabs" };

    void CopyFolder(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        std::filesystem::create_directories(to.parent_path());
        std::filesystem::copy(from, to, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
        std::filesystem::path meta = from.string() + ".meta";
        if (std::filesystem::exists(meta))
        {
            std::filesystem::copy_file(meta, to.string() + ".meta", std::filesystem::copy_options::overwrite_existing);
        }
    }

    Core::Guid FindAsset(const std::string& path)
    {
        const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecordByPath(path);
        return record ? record->guid : Core::Guid{};
    }

    LevelObject* AddPrimitive(Level::Level& level, const Core::Guid& model, const std::string& name, const Math::Vector3& position, const Math::Vector3& scale, const Core::Guid& material, LevelObject* parent = nullptr)
    {
        LevelObject* object = level.CreateObject(LevelObject::StaticType(), nullptr, parent);
        object->SetName(name);
        object->GetTransform().SetLocalPosition(position);
        object->GetTransform().SetLocalScale(scale);
        auto* renderer = object->AddComponent<Rendering::MeshRenderer>();
        renderer->SetModel(Assets::AssetReference<Geometry::ModelAsset>(model));
        renderer->SetMaterial(0, Assets::AssetReference<Rendering::MaterialAsset>(material));
        return object;
    }

    Core::Guid AuthorPlayer(const Reflection::TypeInfo& playerType)
    {
        Level::Level authoring;
        LevelObject* hero = authoring.CreateObject(playerType);
        hero->SetName("Player");
        auto* capsule = hero->AddComponent<Physics::CapsuleCollider>();
        capsule->SetSize(0.4f, 1.8f);
        capsule->SetCenter({ 0.0f, 0.9f, 0.0f });
        hero->AddComponent<Physics::Rigidbody>()->SetMotionType(Physics::MotionType::Kinematic);
        AddPrimitive(authoring, Rendering::BuiltinAssets::Capsule(), "Body", { 0.0f, 0.9f, 0.0f }, { 0.8f, 0.9f, 0.8f }, FindAsset("Materials/Hero.material"), hero);
        auto* camera = authoring.CreateObject<Framework::Camera>(nullptr, hero);
        camera->SetName("PlayerCamera");
        camera->GetTransform().SetLocalPosition({ 0.0f, 2.2f, -5.0f });
        camera->SetActiveScreen(Assets::AssetReference<UI::ScreenAsset>(FindAsset("UI/Hud.screen.xml")));
        Serialization::JsonDocument document;
        Level::LevelSerializer::MakeLevelObjectDocument(*hero, document, Assets::AssetType::LevelObjectVariant);
        return Assets::AssetDatabase::Get().CreateJsonAsset("Prefabs/Player.variant", document);
    }

    void AuthorGameplaySettings(Level::Level& level, const Core::Guid& player)
    {
        using Framework::GameplaySlot;
        using Framework::LevelGameplaySettings;
        Serialization::JsonDocument& settings = level.GetSettings();
        auto& allocator = settings.GetAllocator();
        Serialization::JsonValue& gameMode = LevelGameplaySettings::GetOrCreateEntry(settings, GameplaySlot::GameMode);
        LevelGameplaySettings::SetClassName(gameMode, "ArenaGameMode", allocator);
        Serialization::SetMember(LevelGameplaySettings::GetObjectSettings(gameMode, allocator), "m_defaultPlayable", Serialization::MakeString(player.ToString(), allocator), allocator);
        LevelGameplaySettings::SetClassName(LevelGameplaySettings::GetOrCreateEntry(settings, GameplaySlot::LevelBehavior), "ArenaLevelBehavior", allocator);
    }

    void AuthorArena(Level::Level& level)
    {
        Core::Guid floorMaterial = FindAsset("Materials/Floor.material");
        Core::Guid crateMaterial = FindAsset("Materials/Crate.material");
        Core::Guid metalMaterial = FindAsset("Materials/Metal.material");
        level.DestroyObjectImmediate(level.FindObjectByName("Ground"));
        LevelObject* floor = AddPrimitive(level, Rendering::BuiltinAssets::Plane(), "Floor", { 0, 0, 0 }, { 3, 1, 3 }, floorMaterial);
        auto* floorCollider = floor->AddComponent<Physics::BoxCollider>();
        floorCollider->SetSize({ 10.0f, 0.2f, 10.0f });
        floorCollider->SetCenter({ 0.0f, -0.1f, 0.0f });
        for (int i = 0; i < 4; ++i)
        {
            float angle = static_cast<float>(i) * Math::HalfPi;
            Math::Vector3 position(std::sin(angle) * 15.0f, 1.5f, std::cos(angle) * 15.0f);
            LevelObject* wall = AddPrimitive(level, Rendering::BuiltinAssets::Cube(), "Wall", position, { 30.0f, 3.0f, 0.5f }, metalMaterial);
            wall->GetTransform().SetLocalEulerAngles({ 0.0f, angle * Math::RadiansToDegrees, 0.0f });
            wall->AddComponent<Physics::BoxCollider>();
        }
        for (int i = 0; i < 12; ++i)
        {
            Math::Vector3 position(-4.0f + static_cast<float>(i % 4) * 2.2f, 1.0f + static_cast<float>(i / 4) * 1.6f, 5.0f + static_cast<float>(i % 3) * 0.4f);
            LevelObject* crate = AddPrimitive(level, Rendering::BuiltinAssets::Cube(), "Crate", position, { 1, 1, 1 }, crateMaterial);
            crate->GetTransform().SetLocalEulerAngles({ 0.0f, static_cast<float>(i * 17), 0.0f });
            crate->AddComponent<Physics::BoxCollider>();
            crate->AddComponent<Physics::Rigidbody>()->SetMass(2.0f);
        }
        LevelObject* orb = AddPrimitive(level, Rendering::BuiltinAssets::Sphere(), "Orb", { 4.0f, 1.2f, 2.0f }, { 1.4f, 1.4f, 1.4f }, metalMaterial);
        orb->AddComponent<Physics::SphereCollider>();
        LevelObject* lamp = level.CreateObject(LevelObject::StaticType());
        lamp->SetName("Lamp");
        lamp->GetTransform().SetLocalPosition({ 3.0f, 2.5f, 1.0f });
        auto* lampLight = lamp->AddComponent<Rendering::Light>();
        lampLight->SetLightType(Rendering::LightType::Point);
        lampLight->SetColor({ 1.0f, 0.6f, 0.25f, 1.0f });
        lampLight->SetIntensity(40.0f);
        lampLight->SetRange(8.0f);
        LevelObject* start = level.CreateObject(Framework::PlayerStart::StaticType());
        start->SetName("PlayerStart");
        start->GetTransform().SetLocalPosition({ 0.0f, 0.0f, -4.0f });
    }

    bool Compile(Editor::HotReload& reload)
    {
        reload.RequestCompile();
        auto start = std::chrono::steady_clock::now();
        do
        {
            reload.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } while ((reload.IsCompiling() || reload.GetState() == Editor::CompileState::Idle) && std::chrono::steady_clock::now() - start < std::chrono::minutes(5));
        return reload.GetState() == Editor::CompileState::Succeeded;
    }

    constexpr const char* PackagedFrames = "150";
    constexpr const char* ScreenshotName = "BrightBox3D_ArenaDemo.png";
    constexpr const char* const GameplayTypes[] = { "ArenaGameMode", "ArenaGameState", "ArenaLevelBehavior" };
    constexpr const char* const ExpectedLogLines[] = { "Game started with GameMode 'ArenaGameMode'", "Arena ready" };

    void ExportAuthoredAssets(const std::filesystem::path& root, const std::filesystem::path& sample)
    {
        for (const char* folder : AuthoredFolders)
        {
            std::filesystem::remove_all(sample / folder);
            CopyFolder(root / folder, sample / folder);
        }
        std::filesystem::copy_file(root / Runtime::Project::FileName, sample / Runtime::Project::FileName, std::filesystem::copy_options::overwrite_existing);
    }
}

class SystemTests : public ::testing::Test
{
protected:
    void TearDown() override
    {
        m_reload.reset();
        m_context.reset();
        std::error_code ignored;
        std::filesystem::remove_all(m_root, ignored);
    }

    void CreateProject()
    {
        m_root = Tests::MakeTempDirectory("ArenaDemo");
        std::string error;
        ASSERT_TRUE(Runtime::Project::Create(m_root, "ArenaDemo", error)) << error;
        for (const char* folder : CopiedFolders)
        {
            CopyFolder(SampleRoot / folder, m_root / folder);
        }
        m_context = std::make_unique<Editor::EditorContext>();
        ASSERT_TRUE(m_context->GetProjectService().OpenProject(m_root, error)) << error;
    }

    void CompileGameCode()
    {
        m_reload = std::make_unique<Editor::HotReload>(*m_context);
        ASSERT_TRUE(Compile(*m_reload)) << m_reload->GetStatusText();
        ASSERT_NE(Reflection::TypeRegistry::Find("ArenaPlayer"), nullptr);
        for (const char* type : GameplayTypes)
        {
            ASSERT_NE(Reflection::TypeRegistry::Find(type), nullptr) << type;
        }
    }

    void AuthorLevel()
    {
        Level::Level* level = m_context->GetLevelService().GetEditLevel();
        AuthorGameplaySettings(*level, AuthorPlayer(*Reflection::TypeRegistry::Find("ArenaPlayer")));
        AuthorArena(*level);
        level->GetDefaultCollection()->SetDirty(true);
        ASSERT_TRUE(m_context->GetLevelService().SaveLevel());
    }

    void BuildGame()
    {
        std::string error;
        m_output = Editor::GameBuilder::DefaultOutput(*m_context);
        ASSERT_TRUE(Editor::GameBuilder::Build(*m_context, m_output, error)) << error;
        EXPECT_TRUE(std::filesystem::exists(m_output / "Game.json"));
        EXPECT_TRUE(std::filesystem::exists(m_output / "Cooked" / "Manifest.json"));
        EXPECT_TRUE(std::filesystem::exists(m_output / "EngineContent" / "Shaders" / "GBuffer.hlsl"));
        EXPECT_TRUE(std::filesystem::exists(m_output / Core::SharedLibrary::PlatformFileName("ArenaDemo")));
        m_reload->OnProjectClosed();
        m_context->GetProjectService().CloseProject();
        if (std::getenv("BRIGHT_EXPORT_SAMPLE"))
        {
            ExportAuthoredAssets(m_root, SampleRoot);
        }
    }

    Core::ProcessResult RunPackagedGame(const std::filesystem::path& screenshot)
    {
        std::error_code ignored;
        std::filesystem::remove(screenshot, ignored);
        return Core::Process::Run({ (m_output / "ArenaDemo").string(), "--frames", PackagedFrames, "--screenshot", screenshot.string() }, m_output);
    }

    void VerifyRun(const Core::ProcessResult& run, const std::filesystem::path& screenshot)
    {
        EXPECT_EQ(run.exitCode, 0) << run.output;
        EXPECT_TRUE(std::filesystem::exists(screenshot)) << run.output;
        EXPECT_EQ(run.output.find("[Error]"), std::string::npos) << run.output;
        for (const char* line : ExpectedLogLines)
        {
            EXPECT_NE(run.output.find(line), std::string::npos) << run.output;
        }
    }

    std::filesystem::path m_root;
    std::filesystem::path m_output;
    std::unique_ptr<Editor::EditorContext> m_context;
    std::unique_ptr<Editor::HotReload> m_reload;
};

TEST_F(SystemTests, ArenaDemoAuthorsBuildsAndRunsPackagedGame)
{
    if (Editor::HotReload::FindCMake().empty())
    {
        GTEST_SKIP() << "CMake is not available";
    }
    ASSERT_NO_FATAL_FAILURE(CreateProject());
    ASSERT_NO_FATAL_FAILURE(CompileGameCode());
    ASSERT_NO_FATAL_FAILURE(AuthorLevel());
    ASSERT_NO_FATAL_FAILURE(BuildGame());
    std::filesystem::path screenshot = Tests::TestOutputPath(ScreenshotName);
    VerifyRun(RunPackagedGame(screenshot), screenshot);
}
