#include "Framework/Camera.h"
#include "Framework/FrameworkTestTypes.h"
#include "Framework/GameSession.h"
#include "Framework/LevelGameplaySettings.h"
#include "Level/ObjectReplacer.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace BrightBox3D;
using namespace BrightBox3D::Tests;
using Framework::GameplaySlot;
using Framework::LevelGameplaySettings;
using RuntimeLevel = BrightBox3D::Level::Level;

namespace BrightBox3D::Tests
{
    std::vector<std::string>& FrameworkLog()
    {
        static std::vector<std::string> log;
        return log;
    }
}

namespace
{
    void UseClasses(RuntimeLevel& level, const std::string& gameMode, const std::string& levelBehavior)
    {
        Serialization::JsonDocument& settings = level.GetSettings();
        LevelGameplaySettings::SetClassName(LevelGameplaySettings::GetOrCreateEntry(settings, GameplaySlot::GameMode), gameMode, settings.GetAllocator());
        LevelGameplaySettings::SetClassName(LevelGameplaySettings::GetOrCreateEntry(settings, GameplaySlot::LevelBehavior), levelBehavior, settings.GetAllocator());
    }

    std::vector<std::string> Filter(const std::string& suffix)
    {
        std::vector<std::string> result;
        std::copy_if(FrameworkLog().begin(), FrameworkLog().end(), std::back_inserter(result), [&](const std::string& entry) { return entry.size() >= suffix.size() && entry.compare(entry.size() - suffix.size(), suffix.size(), suffix) == 0; });
        return result;
    }
}

TEST(FrameworkTests, PossessUnpossessAndSwitchBodies)
{
    RuntimeLevel level;
    auto* controller = level.CreateEngineObject<Framework::PlayerController>();
    auto* first = level.CreateObject<TestPlayable>();
    auto* second = level.CreateObject<TestPlayable>();

    controller->Possess(*first);
    EXPECT_EQ(controller->GetPlayable(), first);
    EXPECT_EQ(first->GetController(), controller);
    EXPECT_EQ(first->m_possessions, 1);
    ASSERT_NE(controller->GetViewTarget(), nullptr);
    EXPECT_EQ(controller->GetViewTarget()->GetParent(), first);

    controller->UnPossess();
    EXPECT_EQ(controller->GetPlayable(), nullptr);
    EXPECT_EQ(first->GetController(), nullptr);
    EXPECT_EQ(first->m_unpossessions, 1);
    EXPECT_NE(controller->GetViewTarget(), nullptr);

    controller->Possess(*first);
    controller->Possess(*second);
    EXPECT_EQ(controller->GetPlayable(), second);
    EXPECT_EQ(first->GetController(), nullptr);
    EXPECT_EQ(second->GetController(), controller);
    EXPECT_EQ(first->m_unpossessions, 2);
}

TEST(FrameworkTests, DestroyedPlayableIsRespawnedByTheGameMode)
{
    RuntimeLevel level;
    UseClasses(level, "RecordingGameMode", "");
    auto* gameMode = static_cast<RecordingGameMode*>(Framework::GameSession::Start(level));
    ASSERT_NE(gameMode, nullptr);
    level.Update(0.016f);
    Framework::PlayerController* controller = gameMode->GetPlayerController(0);
    ASSERT_NE(controller, nullptr);
    Framework::PlayableLevelObject* original = controller->GetPlayable();
    ASSERT_NE(original, nullptr);
    EXPECT_EQ(gameMode->m_spawns, 1);

    original->Destroy();
    level.Update(0.016f);
    EXPECT_EQ(gameMode->m_spawns, 2);
    ASSERT_NE(controller->GetPlayable(), nullptr);
    EXPECT_NE(controller->GetPlayable(), original);
    EXPECT_EQ(controller->GetPlayable()->GetController(), controller);
    level.EndPlay();
}

TEST(FrameworkTests, StartAndEndRunInSpecOrder)
{
    FrameworkLog().clear();
    RuntimeLevel level;
    UseClasses(level, "RecordingGameMode", "RecordingLevelBehavior");
    Framework::GameSession::Start(level);
    level.Update(0.016f);
    level.EndPlay();
    EXPECT_EQ(Filter(":Load"), (std::vector<std::string>{ "LevelBehavior:Load", "GameMode:Load", "GameState:Load" }));
    EXPECT_EQ(Filter(":Start"), (std::vector<std::string>{ "LevelBehavior:Start", "GameMode:Start", "GameState:Start" }));
    EXPECT_EQ(Filter(":End"), (std::vector<std::string>{ "GameState:End", "GameMode:End", "LevelBehavior:End" }));
}

TEST(FrameworkTests, HotReloadKeepsGameStateValuesAndPossession)
{
    RuntimeLevel level;
    UseClasses(level, "RecordingGameMode", "");
    Framework::GameModeBase* gameMode = Framework::GameSession::Start(level);
    level.Update(0.016f);
    static_cast<RecordingGameState*>(gameMode->GetGameState())->m_value = 42;
    Core::Guid playableId = gameMode->GetPlayerController(0)->GetPlayable()->GetId();

    std::vector<const Reflection::TypeInfo*> replaced = { &RecordingGameState::StaticType(), &RecordingGameMode::StaticType(), &TestPlayable::StaticType(), &Framework::PlayerController::StaticType() };
    BrightBox3D::Level::ReplacementBatch batch;
    BrightBox3D::Level::ObjectReplacer::Extract(level, [&](const Reflection::TypeInfo& type) { return std::find(replaced.begin(), replaced.end(), &type) != replaced.end(); }, batch);
    BrightBox3D::Level::ObjectReplacer::DestroyExtracted(batch);
    BrightBox3D::Level::ObjectReplacer::Restore(batch);

    Framework::GameModeBase* reloaded = Framework::GameModeBase::Get(level);
    ASSERT_NE(reloaded, nullptr);
    auto* state = Reflection::Cast<RecordingGameState>(reloaded->GetGameState());
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->m_value, 42);
    EXPECT_EQ(state->GetGameMode(), reloaded);
    Framework::PlayerController* controller = reloaded->GetPlayerController(0);
    ASSERT_NE(controller, nullptr);
    ASSERT_NE(controller->GetPlayable(), nullptr);
    EXPECT_EQ(controller->GetPlayable()->GetId(), playableId);
    EXPECT_EQ(controller->GetPlayable()->GetController(), controller);
    EXPECT_EQ(controller->GetPlayable(), level.FindObject(playableId));
    level.Update(0.016f);
    level.EndPlay();
}
