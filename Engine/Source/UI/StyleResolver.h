#pragma once

#include "UI/Style.h"

namespace BrightBox3D::UI
{
    class Element;
    class StyleSheet;
}

namespace BrightBox3D::UI::StyleResolver
{
    void ComputeStyles(Element& element, const StyleSheet& styles, const ComputedStyle* parent = nullptr);
}
