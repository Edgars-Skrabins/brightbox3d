#include "Platform/Window.h"

#include "Core/Log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3native.h>

namespace BrightBox3D::Platform
{
    namespace
    {
        bool s_initialized = false;

        Window* FromHandle(GLFWwindow* handle)
        {
            return static_cast<Window*>(glfwGetWindowUserPointer(handle));
        }

        void ErrorCallback(int code, const char* description)
        {
            Core::Log::Error("GLFW error " + std::to_string(code) + ": " + description);
        }
    }

    bool Window::InitializePlatform()
    {
        if (s_initialized)
        {
            return true;
        }
        glfwSetErrorCallback(&ErrorCallback);
        if (!glfwInit())
        {
            return false;
        }
        s_initialized = true;
        return true;
    }

    void Window::ShutdownPlatform()
    {
        if (s_initialized)
        {
            glfwTerminate();
            s_initialized = false;
        }
    }

    void Window::PollEvents()
    {
        glfwPollEvents();
    }

    void Window::WaitEvents(double timeoutSeconds)
    {
        glfwWaitEventsTimeout(timeoutSeconds);
    }

    double Window::GetTime()
    {
        return glfwGetTime();
    }

    std::string Window::GetClipboardText()
    {
        const char* text = glfwGetClipboardString(nullptr);
        return text ? text : "";
    }

    void Window::SetClipboardText(const std::string& text)
    {
        glfwSetClipboardString(nullptr, text.c_str());
    }

    Window::Window(const WindowDesc& desc)
    {
        if (!InitializePlatform())
        {
            Core::Log::Error("Failed to initialize the platform layer");
            return;
        }

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, desc.visible ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_MAXIMIZED, desc.maximized ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, desc.decorated ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
        m_window = glfwCreateWindow(desc.width, desc.height, desc.title.c_str(), nullptr, nullptr);
        if (!m_window)
        {
            Core::Log::Error("Failed to create window '" + desc.title + "'");
            return;
        }
        glfwSetWindowUserPointer(m_window, this);

        glfwSetKeyCallback(m_window, [](GLFWwindow* handle, int key, int scancode, int action, int modifiers)
        {
            Window* window = FromHandle(handle);
            KeyEvent event;
            event.key = static_cast<Key>(key);
            event.scancode = scancode;
            event.down = action != GLFW_RELEASE;
            event.repeat = action == GLFW_REPEAT;
            event.modifiers = modifiers;
            window->m_input.SetKey(event.key, event.down);
            window->OnKey.Broadcast(event);
        });

        glfwSetMouseButtonCallback(m_window, [](GLFWwindow* handle, int button, int action, int modifiers)
        {
            Window* window = FromHandle(handle);
            MouseButtonEvent event;
            event.button = static_cast<MouseButton>(button);
            event.down = action == GLFW_PRESS;
            event.modifiers = modifiers;
            window->m_input.SetMouseButton(event.button, event.down);
            window->OnMouseButton.Broadcast(event);
        });

        glfwSetCursorPosCallback(m_window, [](GLFWwindow* handle, double x, double y)
        {
            Window* window = FromHandle(handle);
            Math::Vector2 position(static_cast<float>(x), static_cast<float>(y));
            window->m_input.SetMousePosition(position);
            window->OnMouseMove.Broadcast(position);
        });

        glfwSetScrollCallback(m_window, [](GLFWwindow* handle, double x, double y)
        {
            Window* window = FromHandle(handle);
            Math::Vector2 scroll(static_cast<float>(x), static_cast<float>(y));
            window->m_input.AddScroll(scroll);
            window->OnScroll.Broadcast(scroll);
        });

        glfwSetCharCallback(m_window, [](GLFWwindow* handle, unsigned int codepoint)
        {
            Window* window = FromHandle(handle);
            window->m_input.AddText(codepoint);
            window->OnCharacter.Broadcast(codepoint);
        });

        glfwSetWindowFocusCallback(m_window, [](GLFWwindow* handle, int focused)
        {
            Window* window = FromHandle(handle);
            if (!focused)
            {
                window->m_input.Reset();
            }
            window->OnFocus.Broadcast(focused != 0);
        });

        glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* handle, int width, int height)
        {
            FromHandle(handle)->OnFramebufferResize.Broadcast(Math::Vector2(static_cast<float>(width), static_cast<float>(height)));
        });

        glfwSetDropCallback(m_window, [](GLFWwindow* handle, int count, const char** paths)
        {
            std::vector<std::filesystem::path> files;
            for (int i = 0; i < count; ++i)
            {
                files.emplace_back(paths[i]);
            }
            FromHandle(handle)->OnFileDrop.Broadcast(files);
        });

        glfwSetWindowCloseCallback(m_window, [](GLFWwindow* handle)
        {
            FromHandle(handle)->OnCloseRequested.Broadcast();
        });

        int shapes[9] = { GLFW_ARROW_CURSOR, GLFW_IBEAM_CURSOR, GLFW_RESIZE_ALL_CURSOR, GLFW_RESIZE_EW_CURSOR, GLFW_RESIZE_NS_CURSOR, GLFW_RESIZE_NESW_CURSOR, GLFW_RESIZE_NWSE_CURSOR, GLFW_POINTING_HAND_CURSOR, GLFW_NOT_ALLOWED_CURSOR };
        for (int i = 0; i < 9; ++i)
        {
            m_cursors[i] = glfwCreateStandardCursor(shapes[i]);
        }
    }

    Window::~Window()
    {
        for (void* cursor : m_cursors)
        {
            if (cursor)
            {
                glfwDestroyCursor(static_cast<GLFWcursor*>(cursor));
            }
        }
        if (m_window)
        {
            glfwDestroyWindow(m_window);
        }
    }

    bool Window::ShouldClose() const
    {
        return !m_window || glfwWindowShouldClose(m_window);
    }

    void Window::SetShouldClose(bool shouldClose)
    {
        glfwSetWindowShouldClose(m_window, shouldClose ? GLFW_TRUE : GLFW_FALSE);
    }

    void Window::SetTitle(const std::string& title)
    {
        glfwSetWindowTitle(m_window, title.c_str());
    }

    void Window::Show()
    {
        glfwShowWindow(m_window);
    }

    void Window::Hide()
    {
        glfwHideWindow(m_window);
    }

    void Window::Focus()
    {
        glfwFocusWindow(m_window);
    }

    bool Window::IsFocused() const
    {
        return glfwGetWindowAttrib(m_window, GLFW_FOCUSED) != 0;
    }

    bool Window::IsMinimized() const
    {
        return glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0;
    }

    bool Window::IsVisible() const
    {
        return glfwGetWindowAttrib(m_window, GLFW_VISIBLE) != 0;
    }

    Math::Vector2 Window::GetSize() const
    {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(m_window, &width, &height);
        return { static_cast<float>(width), static_cast<float>(height) };
    }

    Math::Vector2 Window::GetFramebufferSize() const
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        return { static_cast<float>(width), static_cast<float>(height) };
    }

    Math::Vector2 Window::GetContentScale() const
    {
        float x = 1.0f;
        float y = 1.0f;
        glfwGetWindowContentScale(m_window, &x, &y);
        return { x, y };
    }

    Math::Vector2 Window::GetPosition() const
    {
        int x = 0;
        int y = 0;
        glfwGetWindowPos(m_window, &x, &y);
        return { static_cast<float>(x), static_cast<float>(y) };
    }

    void Window::SetPosition(const Math::Vector2& position)
    {
        glfwSetWindowPos(m_window, static_cast<int>(position.x), static_cast<int>(position.y));
    }

    void Window::SetSize(const Math::Vector2& size)
    {
        glfwSetWindowSize(m_window, static_cast<int>(size.x), static_cast<int>(size.y));
    }

    void Window::SetCursorMode(CursorMode mode)
    {
        m_cursorMode = mode;
        int value = GLFW_CURSOR_NORMAL;
        if (mode == CursorMode::Hidden)
        {
            value = GLFW_CURSOR_HIDDEN;
        }
        else if (mode == CursorMode::Locked)
        {
            value = GLFW_CURSOR_DISABLED;
        }
        glfwSetInputMode(m_window, GLFW_CURSOR, value);
        if (mode == CursorMode::Locked && glfwRawMouseMotionSupported())
        {
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    }

    void Window::SetCursorShape(CursorShape shape)
    {
        glfwSetCursor(m_window, static_cast<GLFWcursor*>(m_cursors[static_cast<int>(shape)]));
    }

    void* Window::GetNativeHandle() const
    {
#if defined(_WIN32)
        return glfwGetWin32Window(m_window);
#elif defined(__APPLE__)
        return glfwGetCocoaWindow(m_window);
#else
        return nullptr;
#endif
    }

    void* Window::GetNativeView() const
    {
#if defined(__APPLE__)
        return glfwGetCocoaView(m_window);
#else
        return GetNativeHandle();
#endif
    }
}
