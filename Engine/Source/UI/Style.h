#pragma once

#include "Math/Color.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::UI
{
    class Element;

    enum class LengthUnit
    {
        Undefined,
        Auto,
        Pixels,
        Percent
    };

    struct Length
    {
        LengthUnit unit = LengthUnit::Undefined;
        float value = 0.0f;

        static Length Pixels(float value) { return { LengthUnit::Pixels, value }; }
        static Length Percent(float value) { return { LengthUnit::Percent, value }; }
        static Length Auto() { return { LengthUnit::Auto, 0.0f }; }
        bool IsSet() const { return unit != LengthUnit::Undefined; }
    };

    enum class TextAlign
    {
        Left,
        Center,
        Right
    };

    struct ComputedStyle
    {
        bool displayNone = false;
        std::string flexDirection = "column";
        std::string justifyContent = "flex-start";
        std::string alignItems = "stretch";
        std::string alignSelf = "auto";
        std::string alignContent = "flex-start";
        std::string flexWrap = "nowrap";
        std::string position = "relative";
        std::string overflow = "visible";
        float flexGrow = 0.0f;
        float flexShrink = 1.0f;
        Length flexBasis = Length::Auto();
        Length width;
        Length height;
        Length minWidth;
        Length minHeight;
        Length maxWidth;
        Length maxHeight;
        Length margin[4];
        Length padding[4];
        Length inset[4];
        float gap = 0.0f;
        float borderWidth = 0.0f;
        float borderRadius = 0.0f;
        Math::Color backgroundColor = Math::Color::Clear();
        Math::Color borderColor = Math::Color::Clear();
        Math::Color color = Math::Color::White();
        Math::Color fillColor{ 0.25f, 0.55f, 0.95f, 1.0f };
        float fontSize = 16.0f;
        std::string fontFamily;
        TextAlign textAlign = TextAlign::Left;
        float opacity = 1.0f;
    };

    struct SimpleSelector
    {
        std::string type;
        std::string id;
        std::vector<std::string> classes;
        std::vector<std::string> pseudo;
    };

    struct Selector
    {
        std::vector<SimpleSelector> parts;
        int specificity = 0;
    };

    struct StyleRule
    {
        std::vector<Selector> selectors;
        std::vector<std::pair<std::string, std::string>> declarations;
        uint32_t order = 0;
    };

    class StyleSheet
    {
    public:
        bool Parse(const std::string& text, std::string& error);
        const std::vector<StyleRule>& GetRules() const { return m_rules; }
        void Append(const StyleSheet& other);

    private:
        std::vector<StyleRule> m_rules;
    };

    std::vector<std::pair<std::string, std::string>> ParseDeclarations(const std::string& text);
    bool Matches(const Selector& selector, const Element& element);
    void ApplyDeclaration(ComputedStyle& style, const std::string& property, const std::string& value);
    bool ParseColor(const std::string& text, Math::Color& color);
    Length ParseLength(const std::string& text);
}
