#include "ImGui/ImGuiPlatform.h"

#include <imgui.h>

#include <string>

namespace BrightBox3D::Editor
{
    namespace
    {
        ImGuiKey ToImGuiKey(Platform::Key key)
        {
            using Platform::Key;
            switch (key)
            {
            case Key::Tab: return ImGuiKey_Tab;
            case Key::Left: return ImGuiKey_LeftArrow;
            case Key::Right: return ImGuiKey_RightArrow;
            case Key::Up: return ImGuiKey_UpArrow;
            case Key::Down: return ImGuiKey_DownArrow;
            case Key::PageUp: return ImGuiKey_PageUp;
            case Key::PageDown: return ImGuiKey_PageDown;
            case Key::Home: return ImGuiKey_Home;
            case Key::End: return ImGuiKey_End;
            case Key::Insert: return ImGuiKey_Insert;
            case Key::Delete: return ImGuiKey_Delete;
            case Key::Backspace: return ImGuiKey_Backspace;
            case Key::Space: return ImGuiKey_Space;
            case Key::Enter: return ImGuiKey_Enter;
            case Key::Escape: return ImGuiKey_Escape;
            case Key::Apostrophe: return ImGuiKey_Apostrophe;
            case Key::Comma: return ImGuiKey_Comma;
            case Key::Minus: return ImGuiKey_Minus;
            case Key::Period: return ImGuiKey_Period;
            case Key::Slash: return ImGuiKey_Slash;
            case Key::Semicolon: return ImGuiKey_Semicolon;
            case Key::Equal: return ImGuiKey_Equal;
            case Key::LeftBracket: return ImGuiKey_LeftBracket;
            case Key::Backslash: return ImGuiKey_Backslash;
            case Key::RightBracket: return ImGuiKey_RightBracket;
            case Key::GraveAccent: return ImGuiKey_GraveAccent;
            case Key::CapsLock: return ImGuiKey_CapsLock;
            case Key::ScrollLock: return ImGuiKey_ScrollLock;
            case Key::NumLock: return ImGuiKey_NumLock;
            case Key::PrintScreen: return ImGuiKey_PrintScreen;
            case Key::Pause: return ImGuiKey_Pause;
            case Key::Keypad0: return ImGuiKey_Keypad0;
            case Key::Keypad1: return ImGuiKey_Keypad1;
            case Key::Keypad2: return ImGuiKey_Keypad2;
            case Key::Keypad3: return ImGuiKey_Keypad3;
            case Key::Keypad4: return ImGuiKey_Keypad4;
            case Key::Keypad5: return ImGuiKey_Keypad5;
            case Key::Keypad6: return ImGuiKey_Keypad6;
            case Key::Keypad7: return ImGuiKey_Keypad7;
            case Key::Keypad8: return ImGuiKey_Keypad8;
            case Key::Keypad9: return ImGuiKey_Keypad9;
            case Key::KeypadDecimal: return ImGuiKey_KeypadDecimal;
            case Key::KeypadDivide: return ImGuiKey_KeypadDivide;
            case Key::KeypadMultiply: return ImGuiKey_KeypadMultiply;
            case Key::KeypadSubtract: return ImGuiKey_KeypadSubtract;
            case Key::KeypadAdd: return ImGuiKey_KeypadAdd;
            case Key::KeypadEnter: return ImGuiKey_KeypadEnter;
            case Key::KeypadEqual: return ImGuiKey_KeypadEqual;
            case Key::LeftShift: return ImGuiKey_LeftShift;
            case Key::LeftControl: return ImGuiKey_LeftCtrl;
            case Key::LeftAlt: return ImGuiKey_LeftAlt;
            case Key::LeftSuper: return ImGuiKey_LeftSuper;
            case Key::RightShift: return ImGuiKey_RightShift;
            case Key::RightControl: return ImGuiKey_RightCtrl;
            case Key::RightAlt: return ImGuiKey_RightAlt;
            case Key::RightSuper: return ImGuiKey_RightSuper;
            case Key::Menu: return ImGuiKey_Menu;
            default:
                break;
            }
            int code = static_cast<int>(key);
            if (code >= static_cast<int>(Key::Digit0) && code <= static_cast<int>(Key::Digit9))
            {
                return static_cast<ImGuiKey>(ImGuiKey_0 + (code - static_cast<int>(Key::Digit0)));
            }
            if (code >= static_cast<int>(Key::A) && code <= static_cast<int>(Key::Z))
            {
                return static_cast<ImGuiKey>(ImGuiKey_A + (code - static_cast<int>(Key::A)));
            }
            if (code >= static_cast<int>(Key::F1) && code <= static_cast<int>(Key::F12))
            {
                return static_cast<ImGuiKey>(ImGuiKey_F1 + (code - static_cast<int>(Key::F1)));
            }
            return ImGuiKey_None;
        }

        void UpdateModifiers(int modifiers)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.AddKeyEvent(ImGuiMod_Ctrl, (modifiers & 0x0002) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (modifiers & 0x0001) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (modifiers & 0x0004) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (modifiers & 0x0008) != 0);
        }

        std::string& ClipboardStorage()
        {
            static std::string text;
            return text;
        }
    }

    ImGuiPlatform::ImGuiPlatform(Platform::Window& window) : m_window(window)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.BackendPlatformName = "BrightBox3D Window";
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

        ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
        platform.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char*
        {
            ClipboardStorage() = Platform::Window::GetClipboardText();
            return ClipboardStorage().c_str();
        };
        platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text)
        {
            Platform::Window::SetClipboardText(text ? text : "");
        };

        m_keyHandle = window.OnKey.Subscribe([](const Platform::KeyEvent& event)
        {
            UpdateModifiers(event.modifiers);
            ImGuiKey key = ToImGuiKey(event.key);
            if (key != ImGuiKey_None)
            {
                ImGui::GetIO().AddKeyEvent(key, event.down);
            }
        });
        m_buttonHandle = window.OnMouseButton.Subscribe([](const Platform::MouseButtonEvent& event)
        {
            UpdateModifiers(event.modifiers);
            int button = static_cast<int>(event.button);
            if (button >= 0 && button < ImGuiMouseButton_COUNT)
            {
                ImGui::GetIO().AddMouseButtonEvent(button, event.down);
            }
        });
        m_moveHandle = window.OnMouseMove.Subscribe([](const Math::Vector2& position)
        {
            ImGui::GetIO().AddMousePosEvent(position.x, position.y);
        });
        m_scrollHandle = window.OnScroll.Subscribe([](const Math::Vector2& scroll)
        {
            ImGui::GetIO().AddMouseWheelEvent(scroll.x, scroll.y);
        });
        m_characterHandle = window.OnCharacter.Subscribe([](uint32_t codepoint)
        {
            ImGui::GetIO().AddInputCharacter(codepoint);
        });
        m_focusHandle = window.OnFocus.Subscribe([](bool focused)
        {
            ImGui::GetIO().AddFocusEvent(focused);
        });
    }

    ImGuiPlatform::~ImGuiPlatform()
    {
        m_window.OnKey.Unsubscribe(m_keyHandle);
        m_window.OnMouseButton.Unsubscribe(m_buttonHandle);
        m_window.OnMouseMove.Unsubscribe(m_moveHandle);
        m_window.OnScroll.Unsubscribe(m_scrollHandle);
        m_window.OnCharacter.Unsubscribe(m_characterHandle);
        m_window.OnFocus.Unsubscribe(m_focusHandle);
    }

    void ImGuiPlatform::NewFrame(float deltaTime)
    {
        ImGuiIO& io = ImGui::GetIO();
        Math::Vector2 size = m_window.GetSize();
        Math::Vector2 framebuffer = m_window.GetFramebufferSize();
        io.DisplaySize = ImVec2(size.x, size.y);
        if (size.x > 0.0f && size.y > 0.0f)
        {
            io.DisplayFramebufferScale = ImVec2(framebuffer.x / size.x, framebuffer.y / size.y);
        }
        io.DeltaTime = deltaTime > 0.0f ? deltaTime : 1.0f / 60.0f;

        if (!(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) && m_window.GetCursorMode() == Platform::CursorMode::Normal)
        {
            switch (ImGui::GetMouseCursor())
            {
            case ImGuiMouseCursor_TextInput:
                m_window.SetCursorShape(Platform::CursorShape::TextInput);
                break;
            case ImGuiMouseCursor_ResizeAll:
                m_window.SetCursorShape(Platform::CursorShape::ResizeAll);
                break;
            case ImGuiMouseCursor_ResizeEW:
                m_window.SetCursorShape(Platform::CursorShape::ResizeHorizontal);
                break;
            case ImGuiMouseCursor_ResizeNS:
                m_window.SetCursorShape(Platform::CursorShape::ResizeVertical);
                break;
            case ImGuiMouseCursor_ResizeNESW:
                m_window.SetCursorShape(Platform::CursorShape::ResizeDiagonalNESW);
                break;
            case ImGuiMouseCursor_ResizeNWSE:
                m_window.SetCursorShape(Platform::CursorShape::ResizeDiagonalNWSE);
                break;
            case ImGuiMouseCursor_Hand:
                m_window.SetCursorShape(Platform::CursorShape::Hand);
                break;
            case ImGuiMouseCursor_NotAllowed:
                m_window.SetCursorShape(Platform::CursorShape::NotAllowed);
                break;
            default:
                m_window.SetCursorShape(Platform::CursorShape::Arrow);
                break;
            }
        }
    }
}
