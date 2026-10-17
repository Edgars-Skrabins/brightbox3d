#include "UI/Document.h"
#include "UI/Markup.h"

#include <gtest/gtest.h>

#include <string>

using namespace BrightBox3D;
using namespace BrightBox3D::UI;

TEST(MarkupTests, EveryElementAndAttributeNameIsFound)
{
    for (int index = 0; index < static_cast<int>(MarkupElement::Count); ++index)
    {
        MarkupElement element = static_cast<MarkupElement>(index);
        EXPECT_EQ(FindMarkupElement(GetMarkupElementInfo(element).name), element);
    }
    for (int index = 0; index < static_cast<int>(MarkupAttribute::Count); ++index)
    {
        MarkupAttribute attribute = static_cast<MarkupAttribute>(index);
        EXPECT_EQ(FindMarkupAttribute(MarkupAttributeName(attribute)), attribute);
    }
    EXPECT_FALSE(FindMarkupElement("Paragraph"));
    EXPECT_FALSE(FindMarkupAttribute("colour"));
}

TEST(MarkupTests, AttributesAreParsedOnceIntoTypedFields)
{
    Document document;
    Core::Result<void> loaded = document.LoadFromText(R"(
        <Screen>
            <TextField id="field" placeholder="Name" />
            <Slider name="volume" min="2" max="8" value="5" />
            <Toggle name="sound" checked="true" disabled="true" />
            <Label name="hidden" visible="false" text="Hi" />
            <Label name="bound" text="Health {Player.m_health}" visible="{Player.m_visible}" />
            <Button name="go" onClick="{Player.Go}">Go</Button>
        </Screen>)");
    ASSERT_TRUE(loaded) << loaded.GetError().GetText();

    EXPECT_EQ(document.Find("field")->GetPlaceholder(), "Name");
    Element* volume = document.Find("volume");
    EXPECT_FLOAT_EQ(volume->GetMinimum(), 2.0f);
    EXPECT_FLOAT_EQ(volume->GetMaximum(), 8.0f);
    EXPECT_FLOAT_EQ(volume->GetValue(), 5.0f);
    EXPECT_TRUE(document.Find("sound")->GetBool());
    EXPECT_TRUE(document.Find("sound")->IsDisabled());
    EXPECT_TRUE(document.Find("hidden")->IsMarkupHidden());
    EXPECT_EQ(document.Find("hidden")->GetText(), "Hi");

    Element* bound = document.Find("bound");
    EXPECT_EQ(bound->GetText(), "");
    ASSERT_NE(bound->GetBinding(BindingTarget::Text), nullptr);
    ASSERT_NE(bound->GetBinding(BindingTarget::Visible), nullptr);
    EXPECT_FALSE(bound->IsMarkupHidden());

    Element* go = document.Find("go");
    EXPECT_EQ(go->GetText(), "Go");
    ASSERT_NE(go->GetBinding(BindingTarget::OnClick), nullptr);
    EXPECT_TRUE(go->GetBinding(BindingTarget::OnClick)->IsSingleBinding());
}
