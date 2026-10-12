#pragma once

#include "Assets/Asset.h"

#include <string>

namespace BrightBox3D::Assets
{
    BrightClass()
    class TextAsset : public Asset
    {
        BrightBody()
    public:
        const std::string& GetText() const { return m_text; }
        void SetText(std::string text) { m_text = std::move(text); }

    private:
        std::string m_text;
    };
}
