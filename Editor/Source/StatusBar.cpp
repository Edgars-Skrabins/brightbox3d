#include "StatusBar.h"

#include "Core/Log.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "Level/Level.h"
#include "Rendering/RenderStatistics.h"

#include <imgui.h>

namespace BrightBox3D::Editor::StatusBar
{
    void Draw(const std::string& deviceName, const Rendering::RenderStatistics& statistics, const Level::Level* level)
    {
        ImGui::TextDisabled("%s", Text(EditorText::StatusBar, deviceName, level ? level->GetObjectCount() : size_t{ 0 }, statistics.drawCalls, statistics.lights, ImGui::GetIO().Framerate).c_str());
        int errors = Core::Log::Count(Core::LogLevel::Error, Core::LogSource::Game) + Core::Log::Count(Core::LogLevel::Error, Core::LogSource::Engine);
        if (errors > 0)
        {
            ImGui::SameLine();
            ImGui::TextColored(EditorColorVec4(EditorColor::Error), "%s", Text(EditorText::StatusBarErrors, errors).c_str());
        }
    }
}
