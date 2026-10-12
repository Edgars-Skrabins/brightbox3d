#pragma once

#include "PlaySession.h"
#include "Platform/Window.h"
#include "RHI/Device.h"

#include <memory>

namespace BrightBox3D::Rendering
{
    class Renderer;
}

namespace BrightBox3D::Editor
{
    class EditorApplication;

    class EditorPlaySession : public PlaySession
    {
    public:
        EditorPlaySession(EditorContext& context, EditorApplication& application);
        ~EditorPlaySession() override;

        bool Start() override;
        void Stop() override;
        void Tick(float deltaTime) override;
        void Render(RHI::CommandList& commands);
        RHI::Swapchain* GetSwapchain() const { return m_swapchain.get(); }
        bool WantsStop() const { return m_wantsStop; }
        Platform::Window* GetWindow() const { return m_window.get(); }
        void FocusWindow();

    protected:
        void BeginGame() override;

    private:
        EditorApplication& m_application;
        std::unique_ptr<Platform::Window> m_window;
        std::unique_ptr<RHI::Swapchain> m_swapchain;
        bool m_wantsStop = false;
    };
}
