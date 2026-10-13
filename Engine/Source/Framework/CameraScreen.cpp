#include "Framework/CameraScreen.h"

#include "Framework/Camera.h"
#include "Framework/GameModeBase.h"
#include "Framework/PlayableLevelObject.h"
#include "Level/Level.h"
#include "Level/LevelBehavior.h"
#include "UI/ScreenRenderer.h"

namespace BrightBox3D::Framework
{
    namespace
    {
        PlayerController* FindViewingController(const Camera& camera, Level::Level& level, GameModeBase* gameMode)
        {
            auto* owner = Reflection::Cast<PlayableLevelObject>(camera.GetOwnerId().IsValid() ? level.FindObject(camera.GetOwnerId()) : nullptr);
            if (owner && owner->GetController())
            {
                return owner->GetController();
            }
            return gameMode ? gameMode->GetPlayerController(0) : nullptr;
        }
    }

    UI::BindingRoots BuildBindingRoots(const Camera& camera, Level::Level& level)
    {
        GameModeBase* gameMode = GameModeBase::Get(level);
        PlayerController* controller = FindViewingController(camera, level, gameMode);
        UI::BindingRoots roots;
        roots.level = &level;
        roots.Set(UI::BindingRoot::Player, controller ? static_cast<Reflection::EngineObject*>(controller->GetPlayable()) : nullptr);
        roots.Set(UI::BindingRoot::GameMode, gameMode);
        roots.Set(UI::BindingRoot::GameState, gameMode ? gameMode->GetGameState() : nullptr);
        roots.Set(UI::BindingRoot::Controller, controller);
        roots.Set(UI::BindingRoot::LevelBehavior, Level::LevelBehavior::Get(level));
        return roots;
    }

    void RenderCameraScreen(RHI::CommandList& commands, Camera& camera, Level::Level& level, RHI::Texture* target, Platform::Window* window)
    {
        UI::ScreenRenderer::Get().RenderScreen(commands, camera.GetId(), camera.GetActiveScreen().guid, BuildBindingRoots(camera, level), target, window);
    }
}
