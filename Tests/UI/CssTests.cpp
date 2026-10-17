#include "UI/CssKeywords.h"
#include "UI/CssProperties.h"
#include "UI/StyleSheet.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace BrightBox3D::UI;

namespace
{
    template <typename Value>
    void ExpectEveryKeywordParses(CssProperty property, std::span<const CssKeyword<Value>> keywords, Value ComputedStyle::* field)
    {
        for (const CssKeyword<Value>& keyword : keywords)
        {
            std::optional<CssDeclaration> declaration = ParseDeclaration(property, keyword.name);
            ASSERT_TRUE(declaration) << CssPropertyName(property) << ": " << keyword.name;
            ComputedStyle style;
            declaration->apply(style);
            EXPECT_EQ(style.*field, keyword.value) << CssPropertyName(property) << ": " << keyword.name;
        }
    }

    ComputedStyle Apply(const std::string& declarations)
    {
        std::vector<std::string> warnings;
        ComputedStyle style;
        for (const CssDeclaration& declaration : ParseDeclarations(declarations, warnings))
        {
            declaration.apply(style);
        }
        EXPECT_TRUE(warnings.empty());
        return style;
    }
}

TEST(CssTests, EveryKeywordParses)
{
    ExpectEveryKeywordParses<Display>(CssProperty::Display, DisplayKeywords, &ComputedStyle::display);
    ExpectEveryKeywordParses<FlexDirection>(CssProperty::FlexDirection, FlexDirectionKeywords, &ComputedStyle::flexDirection);
    ExpectEveryKeywordParses<Justify>(CssProperty::JustifyContent, JustifyKeywords, &ComputedStyle::justifyContent);
    ExpectEveryKeywordParses<Align>(CssProperty::AlignItems, AlignKeywords, &ComputedStyle::alignItems);
    ExpectEveryKeywordParses<Align>(CssProperty::AlignSelf, AlignKeywords, &ComputedStyle::alignSelf);
    ExpectEveryKeywordParses<Align>(CssProperty::AlignContent, AlignKeywords, &ComputedStyle::alignContent);
    ExpectEveryKeywordParses<FlexWrap>(CssProperty::FlexWrap, FlexWrapKeywords, &ComputedStyle::flexWrap);
    ExpectEveryKeywordParses<PositionType>(CssProperty::Position, PositionKeywords, &ComputedStyle::position);
    ExpectEveryKeywordParses<Overflow>(CssProperty::Overflow, OverflowKeywords, &ComputedStyle::overflow);
    ExpectEveryKeywordParses<TextAlign>(CssProperty::TextAlign, TextAlignKeywords, &ComputedStyle::textAlign);
}

TEST(CssTests, EveryPropertyNameIsFound)
{
    for (int index = 0; index < static_cast<int>(CssProperty::Count); ++index)
    {
        CssProperty property = static_cast<CssProperty>(index);
        EXPECT_EQ(FindCssProperty(CssPropertyName(property)), property);
    }
}

TEST(CssTests, ShorthandsAndValuesApply)
{
    ComputedStyle style = Apply("flex: 2 0 10px; padding: 1px 2px 3px; margin-left: 5%; border: 2px #ff0000; JUSTIFY-CONTENT: CENTER");
    EXPECT_FLOAT_EQ(style.flexGrow, 2.0f);
    EXPECT_FLOAT_EQ(style.flexShrink, 0.0f);
    EXPECT_EQ(style.flexBasis.unit, LengthUnit::Pixels);
    EXPECT_FLOAT_EQ(style.padding[BoxLeft].value, 2.0f);
    EXPECT_FLOAT_EQ(style.padding[BoxBottom].value, 3.0f);
    EXPECT_EQ(style.margin[BoxLeft].unit, LengthUnit::Percent);
    EXPECT_FLOAT_EQ(style.borderWidth, 2.0f);
    EXPECT_FLOAT_EQ(style.borderColor.r, 1.0f);
    EXPECT_EQ(style.justifyContent, Justify::Center);
}

TEST(CssTests, UnknownPropertyAndKeywordAreReported)
{
    std::vector<std::string> warnings;
    std::vector<CssDeclaration> declarations = ParseDeclarations("colour: red; justify-content: middle; color: blue", warnings);
    EXPECT_EQ(declarations.size(), 1u);
    ASSERT_EQ(warnings.size(), 2u);
    EXPECT_NE(warnings[0].find("colour"), std::string::npos);
    EXPECT_NE(warnings[1].find("middle"), std::string::npos);
}

TEST(CssTests, UnknownPseudoClassIsReported)
{
    StyleSheet sheet;
    ASSERT_TRUE(sheet.Parse("Button:wobble { color: red; } Button:hover { color: blue; }"));
    ASSERT_EQ(sheet.GetWarnings().size(), 1u);
    EXPECT_NE(sheet.GetWarnings()[0].find("wobble"), std::string::npos);
    EXPECT_EQ(sheet.GetRules()[1].selectors[0].parts[0].pseudo, std::vector<PseudoClass>{ PseudoClass::Hover });
}
