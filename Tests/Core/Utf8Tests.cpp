#include "Core/Utf8.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace BrightBox3D;

TEST(Utf8Tests, EncodeDecodeRoundTripsEveryLength)
{
    std::vector<uint32_t> codepoints = { 0x41, 0xE9, 0x20AC, 0x1F600 };
    std::string text;
    for (uint32_t codepoint : codepoints)
    {
        text += Core::Utf8::Encode(static_cast<char32_t>(codepoint));
    }
    EXPECT_EQ(text, "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80");
    EXPECT_EQ(Core::Utf8::Decode(text), codepoints);
}

TEST(Utf8Tests, PopCodepointRemovesWholeCharacters)
{
    std::string text = "a\xC3\xA9\xF0\x9F\x98\x80";
    Core::Utf8::PopCodepoint(text);
    EXPECT_EQ(text, "a\xC3\xA9");
    Core::Utf8::PopCodepoint(text);
    EXPECT_EQ(text, "a");
    Core::Utf8::PopCodepoint(text);
    EXPECT_TRUE(text.empty());
    Core::Utf8::PopCodepoint(text);
    EXPECT_TRUE(text.empty());
}
