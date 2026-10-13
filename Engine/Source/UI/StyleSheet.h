#pragma once

#include "UI/Style.h"

#include <string>
#include <string_view>
#include <vector>

namespace BrightBox3D::UI
{
    class StyleSheet
    {
    public:
        bool Parse(std::string_view text, std::string& error);
        const std::vector<StyleRule>& GetRules() const { return m_rules; }
        const std::vector<std::string>& GetWarnings() const { return m_warnings; }
        void Append(const StyleSheet& other);

    private:
        std::vector<StyleRule> m_rules;
        std::vector<std::string> m_warnings;
    };
}
