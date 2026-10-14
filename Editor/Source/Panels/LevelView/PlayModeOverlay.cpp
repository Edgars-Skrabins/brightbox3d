#include "Panels/LevelView/PlayModeOverlay.h"

#include "EditorPreferences.h"
#include "EditorText.h"

namespace BrightBox3D::Editor::PlayModeOverlay
{
    namespace
    {
        constexpr float BorderAlpha = 0.8f;
        constexpr float BorderThickness = 2.0f;
        constexpr float BannerAlpha = 0.85f;
        constexpr float BannerMargin = 6.0f;
        constexpr float BannerPaddingX = 6.0f;
        constexpr float BannerPaddingY = 3.0f;
        constexpr float BannerRounding = 4.0f;
    }

    void Draw(const ImVec2& origin, const ImVec2& size)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), EditorColorU32(EditorColor::PlayMode, BorderAlpha), 0.0f, 0, BorderThickness);
        const char* banner = Text(EditorText::PlayModeBanner);
        ImVec2 textSize = ImGui::CalcTextSize(banner);
        ImVec2 bannerMin(origin.x + BannerMargin, origin.y + BannerMargin);
        ImVec2 textPosition(bannerMin.x + BannerPaddingX, bannerMin.y + BannerPaddingY);
        ImVec2 bannerMax(textPosition.x + textSize.x + BannerPaddingX, textPosition.y + textSize.y + BannerPaddingY);
        drawList->AddRectFilled(bannerMin, bannerMax, EditorColorU32(EditorColor::PlayMode, BannerAlpha), BannerRounding);
        drawList->AddText(textPosition, EditorColorU32(EditorColor::Text), banner);
    }
}
