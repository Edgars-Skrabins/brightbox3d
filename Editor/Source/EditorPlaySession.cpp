#include "EditorPlaySession.h"

#include "Core/Log.h"
#include "EditorApplication.h"
#include "Framework/Camera.h"
#include "Framework/GameSession.h"
#include "Platform/Input.h"
#include "Rendering/Renderer.h"
#include "UI/ScreenRenderer.h"

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr uint64_t PlayViewId = 0x504C4159;
    }

    EditorPlaySession::EditorPlaySession(EditorContext& context, EditorApplication& application) : PlaySession(context), m_application(application)
    {
    }

    EditorPlaySession::~EditorPlaySession()
    {
        if (IsActive())
        {
            Stop();
        }
    }

    bool EditorPlaySession::Start()
    {
        m_wantsStop = false;
        if (!PlaySession::Start())
        {
            return false;
        }
        Platform::WindowDesc desc;
        desc.title = m_context.GetProject().GetName() + " - PlayMode";
        desc.width = 1280;
        desc.height = 720;
        m_window = std::make_unique<Platform::Window>(desc);
        if (!m_window->IsValid())
        {
            Core::Log::Error("Could not create the PlayMode window");
            PlaySession::Stop();
            m_window.reset();
            return false;
        }
        m_window->OnCloseRequested.Subscribe([this] { m_wantsStop = true; });
        m_window->OnKey.Subscribe([this](const Platform::KeyEvent& event)
        {
            if (event.down && event.key == Platform::Key::Escape && m_window->GetCursorMode() == Platform::CursorMode::Locked)
            {
                m_window->SetCursorMode(Platform::CursorMode::Normal);
            }
        });
        m_swapchain = m_application.GetDevice().CreateSwapchain(*m_window, true);
        Platform::Input::SetWindow(m_window.get());
        m_window->Focus();
        return true;
    }

    void EditorPlaySession::BeginGame()
    {
        Framework::GameSession::Start(*m_level, m_context.GetProject().GetGameMode());
    }

    void EditorPlaySession::Stop()
    {
        if (!IsActive())
        {
            return;
        }
        m_application.GetDevice().WaitIdle();
        if (m_window)
        {
            m_window->SetCursorMode(Platform::CursorMode::Normal);
        }
        PlaySession::Stop();
        Platform::Input::SetWindow(nullptr);
        m_application.GetRenderer().ReleaseView(PlayViewId);
        m_swapchain.reset();
        m_window.reset();
        m_wantsStop = false;
    }

    void EditorPlaySession::FocusWindow()
    {
        if (m_window)
        {
            m_window->Focus();
        }
    }

    void EditorPlaySession::Tick(float deltaTime)
    {
        if (!IsActive())
        {
            return;
        }
        if (m_window && m_window->ShouldClose())
        {
            m_wantsStop = true;
        }
        if (m_window)
        {
            m_window->GetInput().BeginFrame();
        }
        PlaySession::Tick(deltaTime);
    }

    void EditorPlaySession::Render(RHI::CommandList& commands)
    {
        if (!IsActive() || !m_swapchain || !m_window || m_window->IsMinimized())
        {
            return;
        }
        RHI::Texture* backbuffer = m_swapchain->AcquireNextTexture();
        if (!backbuffer)
        {
            return;
        }
        Framework::Camera* camera = Framework::Camera::FindActive(*m_level);
        if (camera)
        {
            Rendering::RenderView view = camera->BuildRenderView(backbuffer->GetWidth(), backbuffer->GetHeight(), PlayViewId);
            m_application.GetRenderer().Render(commands, m_level.get(), view, backbuffer);
            UI::ScreenRenderer::Get().RenderCameraScreen(commands, *camera, *m_level, backbuffer, m_window.get());
        }
        else
        {
            RHI::RenderPassDesc pass;
            pass.colors.push_back({ backbuffer, 0, 0, RHI::LoadAction::Clear, { 0.05f, 0.05f, 0.06f, 1.0f } });
            commands.BeginRenderPass(pass);
            commands.EndRenderPass();
        }
    }
}
