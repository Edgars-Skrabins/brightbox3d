#pragma once

#include "Core/EngineInfo.h"
#include "Core/Event.h"
#include "Math/Vector2.h"
#include "Platform/InputState.h"

#include <filesystem>
#include <string>
#include <vector>

struct GLFWwindow;

namespace BrightBox3D::Platform
{
    struct WindowDesc
    {
        std::string title = Core::EngineName;
        int width = 1280;
        int height = 720;
        bool resizable = true;
        bool visible = true;
        bool maximized = false;
        bool decorated = true;
    };

    enum class CursorMode
    {
        Normal,
        Hidden,
        Locked
    };

    enum class CursorShape
    {
        Arrow,
        TextInput,
        ResizeAll,
        ResizeHorizontal,
        ResizeVertical,
        ResizeDiagonalNESW,
        ResizeDiagonalNWSE,
        Hand,
        NotAllowed
    };

    struct KeyEvent
    {
        Key key = Key::Unknown;
        int scancode = 0;
        bool down = false;
        bool repeat = false;
        int modifiers = 0;
    };

    struct MouseButtonEvent
    {
        MouseButton button = MouseButton::Left;
        bool down = false;
        int modifiers = 0;
    };

    class Window
    {
    public:
        explicit Window(const WindowDesc& desc);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        bool IsValid() const { return m_window != nullptr; }
        bool ShouldClose() const;
        void SetShouldClose(bool shouldClose);
        void SetTitle(const std::string& title);
        void Show();
        void Hide();
        void Focus();
        bool IsFocused() const;
        bool IsMinimized() const;
        bool IsVisible() const;

        Math::Vector2 GetSize() const;
        Math::Vector2 GetFramebufferSize() const;
        Math::Vector2 GetContentScale() const;
        Math::Vector2 GetPosition() const;
        void SetPosition(const Math::Vector2& position);
        void SetSize(const Math::Vector2& size);

        void SetCursorMode(CursorMode mode);
        CursorMode GetCursorMode() const { return m_cursorMode; }
        void SetCursorShape(CursorShape shape);

        void* GetNativeHandle() const;
        void* GetNativeView() const;
        GLFWwindow* GetHandle() const { return m_window; }
        InputState& GetInput() { return m_input; }
        const InputState& GetInput() const { return m_input; }

        Core::Event<KeyEvent> OnKey;
        Core::Event<MouseButtonEvent> OnMouseButton;
        Core::Event<Math::Vector2> OnMouseMove;
        Core::Event<Math::Vector2> OnScroll;
        Core::Event<uint32_t> OnCharacter;
        Core::Event<bool> OnFocus;
        Core::Event<Math::Vector2> OnFramebufferResize;
        Core::Event<const std::vector<std::filesystem::path>&> OnFileDrop;
        Core::Event<> OnCloseRequested;

        static bool InitializePlatform();
        static void ShutdownPlatform();
        static void PollEvents();
        static void WaitEvents(double timeoutSeconds);
        static double GetTime();
        static std::string GetClipboardText();
        static void SetClipboardText(const std::string& text);

    private:
        GLFWwindow* m_window = nullptr;
        InputState m_input;
        CursorMode m_cursorMode = CursorMode::Normal;
        void* m_cursors[9] = {};
    };
}
