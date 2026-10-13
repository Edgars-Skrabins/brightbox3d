#include "Assets/AssetDatabase.h"
#include "Core/FileSystem.h"
#include "Core/Process.h"
#include "EditorContext.h"
#include "Framework/Camera.h"
#include "Framework/PlayerController.h"
#include "Framework/PlayerStart.h"
#include "GameBuilder.h"
#include "HotReload.h"
#include "Level/LevelSerializer.h"
#include "Physics/Colliders.h"
#include "Physics/Rigidbody.h"
#include "Reflection/TypeRegistry.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/MeshRenderer.h"
#include "Serialization/ObjectSerializer.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>

using namespace BrightBox3D;
using BrightBox3D::Level::LevelObjectInstance;

namespace
{
    const char* PlayerHeader = R"(#pragma once

#include "Level/LevelObjectInstance.h"

BrightClass(Category = "Arena")
class ArenaPlayer : public BrightBox3D::Level::LevelObjectInstance
{
    BrightBody()
public:
    void OnGameStart() override;
    void OnGameUpdate(float deltaTime) override;

private:
    BrightProperty(EditDefaultsOnly, Category = "Movement")
    float m_moveSpeed = 6.0f;

    BrightProperty(EditDefaultsOnly, Category = "Movement")
    float m_lookSensitivity = 0.15f;

    BrightProperty(VisibleAnywhere, Category = "Stats")
    BrightUIProperty(Read)
    int m_health = 100;

    BrightUIProperty(Read)
    float m_maxHealth = 100.0f;

    BrightUIProperty(Write)
    std::string m_playerName = "Arena Hero";
};
)";

    const char* PlayerSource = R"(#include "ArenaPlayer.h"

#include "Framework/PlayerController.h"
#include "Platform/Key.h"

using BrightBox3D::Platform::Key;

void ArenaPlayer::OnGameStart()
{
    m_health = 100;
}

void ArenaPlayer::OnGameUpdate(float deltaTime)
{
    auto* controller = GetComponent<BrightBox3D::Framework::PlayerController>();
    if (!controller)
    {
        return;
    }
    BrightBox3D::Math::Vector2 mouse = controller->GetMouseDelta();
    if (controller->IsKeyDown(Key::LeftShift) || controller->GetCamera())
    {
        controller->AddLookInput(mouse.x * m_lookSensitivity, mouse.y * m_lookSensitivity);
    }
    BrightBox3D::Math::Vector3 input;
    if (controller->IsKeyDown(Key::W)) input.z += 1.0f;
    if (controller->IsKeyDown(Key::S)) input.z -= 1.0f;
    if (controller->IsKeyDown(Key::D)) input.x += 1.0f;
    if (controller->IsKeyDown(Key::A)) input.x -= 1.0f;
    BrightBox3D::Math::Quaternion yaw = BrightBox3D::Math::Quaternion::Euler(0.0f, controller->GetYaw(), 0.0f);
    GetTransform().SetRotation(yaw);
    GetTransform().Translate(input.Normalized() * m_moveSpeed * deltaTime, false);
}
)";

    const char* GameModeHeader = R"(#pragma once

#include "Framework/GameMode.h"

BrightClass(Category = "Arena")
class ArenaGameMode : public BrightBox3D::Framework::GameMode
{
    BrightBody()
public:
    void OnGameUpdate(float deltaTime) override;

private:
    BrightProperty(EditDefaultsOnly, Category = "Rules")
    float m_roundLength = 120.0f;

    BrightUIProperty(Read)
    float m_timeRemaining = 120.0f;

    BrightUIProperty(Write)
    bool m_restartRequested = false;
};
)";

    const char* GameModeSource = R"(#include "ArenaGameMode.h"

void ArenaGameMode::OnGameUpdate(float deltaTime)
{
    m_timeRemaining = m_timeRemaining > deltaTime ? m_timeRemaining - deltaTime : 0.0f;
    if (m_restartRequested)
    {
        m_restartRequested = false;
        m_timeRemaining = m_roundLength;
    }
}
)";

    const char* HudScreen = R"(<Screen>
    <Style src="UI/Hud.css" />
    <Panel class="hud">
        <Panel class="card">
            <Label class="title" text="{Player.m_playerName}" />
            <Label text="Health {Player.m_health} / {Player.m_maxHealth:0}" />
            <ProgressBar class="health" value="{Player.m_health}" max="{Player.m_maxHealth}" />
            <Label class="timer" text="Round ends in {GameMode.m_timeRemaining:0}s" />
            <Button text="Restart Round" onClick="{GameMode.m_restartRequested}" />
        </Panel>
        <Label class="hint" text="WASD to move - mouse to look - Esc to unlock the cursor" />
    </Panel>
</Screen>
)";

    const char* HudStyle = R"(.hud { width: 100%; height: 100%; padding: 20px; justify-content: space-between; align-items: flex-start; }
.card { background-color: #12161bcc; border: 1px #3d7fd6; border-radius: 8px; padding: 14px; gap: 8px; width: 300px; }
.title { font-size: 24px; color: #ffd166; }
.health { width: 100%; fill-color: #4ccf6a; }
.timer { color: #c9d4e0; font-size: 15px; }
.hint { color: #ffffffaa; font-size: 14px; }
)";

    void Write(const std::filesystem::path& path, const std::string& text)
    {
        Core::FileSystem::WriteText(path, text);
    }

    Core::Guid SaveMaterial(const std::string& path, const Math::Color& color, float roughness, float metallic)
    {
        Rendering::MaterialAsset material;
        material.m_baseColor = color;
        material.m_roughness = roughness;
        material.m_metallic = metallic;
        Serialization::JsonDocument document;
        Serialization::SerializeProperties(material, document, document.GetAllocator());
        return Assets::AssetDatabase::Get().CreateJsonAsset(path, document);
    }

    LevelObjectInstance* AddPrimitive(Level::Level& level, const Core::Guid& model, const std::string& name, const Math::Vector3& position, const Math::Vector3& scale, const Core::Guid& material)
    {
        LevelObjectInstance* object = level.CreateObject(LevelObjectInstance::StaticType());
        object->SetName(name);
        object->GetTransform().SetLocalPosition(position);
        object->GetTransform().SetLocalScale(scale);
        auto* renderer = object->AddComponent<Rendering::MeshRenderer>();
        renderer->SetModel(Assets::AssetReference<Rendering::ModelAsset>(model));
        if (material.IsValid())
        {
            renderer->SetMaterial(0, Assets::AssetReference<Rendering::MaterialAsset>(material));
        }
        return object;
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
}

TEST(SystemTests, ArenaDemoAuthorsBuildsAndRunsPackagedGame)
{
    if (Editor::HotReload::FindCMake().empty())
    {
        GTEST_SKIP() << "CMake is not available";
    }
    const char* sampleOutput = std::getenv("BRIGHT_SAMPLE_OUTPUT");
    std::filesystem::path root = sampleOutput ? std::filesystem::path(sampleOutput) : std::filesystem::temp_directory_path() / ("ArenaDemo_" + Core::Guid::Generate().ToString());
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::string error;
    ASSERT_TRUE(Runtime::Project::Create(root, "ArenaDemo", error)) << error;
    Write(root / "Source" / "ArenaPlayer.h", PlayerHeader);
    Write(root / "Source" / "ArenaPlayer.cpp", PlayerSource);
    Write(root / "Source" / "ArenaGameMode.h", GameModeHeader);
    Write(root / "Source" / "ArenaGameMode.cpp", GameModeSource);
    Write(root / "Assets" / "UI" / "Hud.screen.xml", HudScreen);
    Write(root / "Assets" / "UI" / "Hud.css", HudStyle);

    Editor::EditorContext context;
    ASSERT_TRUE(context.OpenProject(root, error)) << error;
    Editor::HotReload reload(context);
    ASSERT_TRUE(Compile(reload)) << reload.GetStatusText();
    const Reflection::TypeInfo* playerType = Reflection::TypeRegistry::Find("ArenaPlayer");
    const Reflection::TypeInfo* gameModeType = Reflection::TypeRegistry::Find("ArenaGameMode");
    ASSERT_NE(playerType, nullptr);
    ASSERT_NE(gameModeType, nullptr);

    Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
    Core::Guid floorMaterial = SaveMaterial("Materials/Floor.material", { 0.32f, 0.34f, 0.38f, 1.0f }, 0.85f, 0.0f);
    Core::Guid crateMaterial = SaveMaterial("Materials/Crate.material", { 0.85f, 0.45f, 0.18f, 1.0f }, 0.5f, 0.0f);
    Core::Guid metalMaterial = SaveMaterial("Materials/Metal.material", { 0.75f, 0.78f, 0.82f, 1.0f }, 0.25f, 1.0f);
    Core::Guid heroMaterial = SaveMaterial("Materials/Hero.material", { 0.2f, 0.55f, 0.95f, 1.0f }, 0.4f, 0.0f);
    const Assets::AssetRecord* hud = database.FindRecordByPath("UI/Hud.screen.xml");
    ASSERT_NE(hud, nullptr);

    Level::Level authoring;
    LevelObjectInstance* cameraObject = authoring.CreateObject(Framework::Camera::StaticType());
    cameraObject->SetName("PlayerCamera");
    auto* camera = static_cast<Framework::Camera*>(cameraObject);
    camera->SetActiveScreen(Assets::AssetReference<UI::ScreenAsset>(hud->guid));
    Serialization::JsonDocument cameraDocument;
    Level::LevelSerializer::MakeLevelObjectDocument(*cameraObject, cameraDocument);
    Core::Guid cameraAsset = database.CreateJsonAsset("Prefabs/PlayerCamera.levelobject", cameraDocument);

    LevelObjectInstance* hero = authoring.CreateObject(*playerType);
    hero->SetName("Player");
    auto* controller = hero->AddComponent<Framework::PlayerController>();
    Reflection::TypeInfo const& controllerType = controller->GetType();
    controllerType.FindProperty("m_cameraLevelObject")->GetValue<Assets::AssetReference<Level::LevelObjectAsset>>(controller).guid = cameraAsset;
    controllerType.FindProperty("m_cameraOffset")->GetValue<Math::Vector3>(controller) = { 0.0f, 2.2f, -5.0f };
    controllerType.FindProperty("m_cameraUsesLookRotation")->GetValue<bool>(controller) = false;
    auto* capsule = hero->AddComponent<Physics::CapsuleCollider>();
    capsule->SetSize(0.4f, 1.8f);
    capsule->SetCenter({ 0.0f, 0.9f, 0.0f });
    hero->AddComponent<Physics::Rigidbody>()->SetMotionType(Physics::MotionType::Kinematic);
    LevelObjectInstance* body = authoring.CreateObject(LevelObjectInstance::StaticType(), nullptr, hero);
    body->SetName("Body");
    body->GetTransform().SetLocalPosition({ 0.0f, 0.9f, 0.0f });
    body->GetTransform().SetLocalScale({ 0.8f, 0.9f, 0.8f });
    auto* bodyRenderer = body->AddComponent<Rendering::MeshRenderer>();
    bodyRenderer->SetModel(Assets::AssetReference<Rendering::ModelAsset>(Rendering::BuiltinAssets::Capsule()));
    bodyRenderer->SetMaterial(0, Assets::AssetReference<Rendering::MaterialAsset>(heroMaterial));
    Serialization::JsonDocument heroDocument;
    Level::LevelSerializer::MakeLevelObjectDocument(*hero, heroDocument);
    Core::Guid heroAsset = database.CreateJsonAsset("Prefabs/Player.levelobject", heroDocument);

    Serialization::JsonDocument& gameMode = context.GetProject().GetGameMode();
    gameMode.SetObject();
    gameMode.AddMember("$type", "ArenaGameMode", gameMode.GetAllocator());
    gameMode.AddMember("m_defaultPlayer", Serialization::MakeString(heroAsset.ToString(), gameMode.GetAllocator()), gameMode.GetAllocator());
    context.GetProject().Save();

    Level::Level* level = context.GetEditLevel();
    level->DestroyObjectImmediate(level->FindObjectByName("Ground"));
    LevelObjectInstance* floor = AddPrimitive(*level, Rendering::BuiltinAssets::Plane(), "Floor", { 0, 0, 0 }, { 3, 1, 3 }, floorMaterial);
    auto* floorCollider = floor->AddComponent<Physics::BoxCollider>();
    floorCollider->SetSize({ 10.0f, 0.2f, 10.0f });
    floorCollider->SetCenter({ 0.0f, -0.1f, 0.0f });
    for (int i = 0; i < 4; ++i)
    {
        float angle = static_cast<float>(i) * Math::HalfPi;
        Math::Vector3 position(std::sin(angle) * 15.0f, 1.5f, std::cos(angle) * 15.0f);
        LevelObjectInstance* wall = AddPrimitive(*level, Rendering::BuiltinAssets::Cube(), "Wall", position, { 30.0f, 3.0f, 0.5f }, metalMaterial);
        wall->GetTransform().SetLocalEulerAngles({ 0.0f, angle * Math::RadiansToDegrees, 0.0f });
        wall->AddComponent<Physics::BoxCollider>();
    }
    for (int i = 0; i < 12; ++i)
    {
        Math::Vector3 position(-4.0f + static_cast<float>(i % 4) * 2.2f, 1.0f + static_cast<float>(i / 4) * 1.6f, 5.0f + static_cast<float>(i % 3) * 0.4f);
        LevelObjectInstance* crate = AddPrimitive(*level, Rendering::BuiltinAssets::Cube(), "Crate", position, { 1, 1, 1 }, crateMaterial);
        crate->GetTransform().SetLocalEulerAngles({ 0.0f, static_cast<float>(i * 17), 0.0f });
        crate->AddComponent<Physics::BoxCollider>();
        crate->AddComponent<Physics::Rigidbody>()->SetMass(2.0f);
    }
    LevelObjectInstance* orb = AddPrimitive(*level, Rendering::BuiltinAssets::Sphere(), "Orb", { 4.0f, 1.2f, 2.0f }, { 1.4f, 1.4f, 1.4f }, metalMaterial);
    orb->AddComponent<Physics::SphereCollider>();
    LevelObjectInstance* lamp = level->CreateObject(LevelObjectInstance::StaticType());
    lamp->SetName("Lamp");
    lamp->GetTransform().SetLocalPosition({ 3.0f, 2.5f, 1.0f });
    auto* lampLight = lamp->AddComponent<Rendering::Light>();
    lampLight->SetLightType(Rendering::LightType::Point);
    lampLight->SetColor({ 1.0f, 0.6f, 0.25f, 1.0f });
    lampLight->SetIntensity(40.0f);
    lampLight->SetRange(8.0f);
    LevelObjectInstance* start = level->CreateObject(Framework::PlayerStart::StaticType());
    start->SetName("PlayerStart");
    start->GetTransform().SetLocalPosition({ 0.0f, 0.0f, -4.0f });
    level->GetDefaultCollection()->SetDirty(true);
    ASSERT_TRUE(context.SaveLevel());

    std::filesystem::path output = Editor::GameBuilder::DefaultOutput(context);
    ASSERT_TRUE(Editor::GameBuilder::Build(context, output, error)) << error;
    EXPECT_TRUE(std::filesystem::exists(output / "Game.json"));
    EXPECT_TRUE(std::filesystem::exists(output / "Cooked" / "Manifest.json"));
    EXPECT_TRUE(std::filesystem::exists(output / "EngineContent" / "Shaders" / "GBuffer.hlsl"));
    EXPECT_TRUE(std::filesystem::exists(output / Core::SharedLibrary::PlatformFileName("ArenaDemo")));

    reload.OnProjectClosed();
    context.CloseProject();

    std::filesystem::path screenshot = std::filesystem::path(std::getenv("BRIGHT_TEST_OUTPUT") ? std::getenv("BRIGHT_TEST_OUTPUT") : std::filesystem::temp_directory_path().string()) / "BrightBox3D_ArenaDemo.png";
    std::filesystem::remove(screenshot, ignored);
    Core::ProcessResult run = Core::Process::Run({ (output / "ArenaDemo").string(), "--frames", "150", "--screenshot", screenshot.string() }, output);
    EXPECT_EQ(run.exitCode, 0) << run.output;
    EXPECT_TRUE(std::filesystem::exists(screenshot)) << run.output;
    EXPECT_EQ(run.output.find("[Error]"), std::string::npos) << run.output;
    EXPECT_NE(run.output.find("Game started with GameMode 'ArenaGameMode'"), std::string::npos) << run.output;
    if (!sampleOutput)
    {
        std::filesystem::remove_all(root, ignored);
    }
}
