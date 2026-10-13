#pragma once

#include "EditorText.h"
#include "IconGlyphs.h"

#include <string>

namespace BrightBox3D::Editor
{
    inline std::string IconLabel(Icon icon, const std::string& text)
    {
        return std::string(IconGlyph(icon)) + "  " + text;
    }

    inline std::string IconLabel(Icon icon, EditorText text)
    {
        return IconLabel(icon, std::string(Text(text)));
    }

    inline std::string LabelWithId(const std::string& text, const char* id)
    {
        return text + "###" + id;
    }

    inline std::string LabelWithId(EditorText text, const char* id)
    {
        return LabelWithId(std::string(Text(text)), id);
    }
}
