#include "Core/Error.h"
#include "Core/Result.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace BrightBox3D;
using Core::EngineText;

namespace
{
    struct Counter
    {
        int value = 0;

        int Next() { return ++value; }
    };

    Core::Result<int> ParsePositive(int value)
    {
        if (value <= 0)
        {
            return Core::Error(EngineText::JsonParseError, "negative", value);
        }
        return value;
    }

    Core::Result<void> Check(bool ok)
    {
        if (!ok)
        {
            return Core::Error(EngineText::CannotReadFile);
        }
        return {};
    }
}

TEST(ResultTests, ErrorKeepsItsTextIdArgumentsAndFormattedText)
{
    Core::Error error(EngineText::JsonParseError, "unexpected token", 12);
    EXPECT_EQ(error.GetId(), EngineText::JsonParseError);
    EXPECT_EQ(error.GetArguments(), (std::vector<std::string>{ "unexpected token", "12" }));
    EXPECT_EQ(error.GetText(), Core::Text(EngineText::JsonParseError, "unexpected token", 12));
}

TEST(ResultTests, ValueOrErrorIsReportedThroughBool)
{
    Core::Result<int> value = ParsePositive(4);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 4);

    Core::Result<int> failure = ParsePositive(-1);
    ASSERT_FALSE(failure);
    EXPECT_EQ(failure.GetError().GetId(), EngineText::JsonParseError);
    EXPECT_EQ(failure.GetError().GetArguments().back(), "-1");
}

TEST(ResultTests, VoidResultCarriesOnlyAnError)
{
    EXPECT_TRUE(Check(true));
    Core::Result<void> failure = Check(false);
    ASSERT_FALSE(failure);
    EXPECT_EQ(failure.GetError().GetId(), EngineText::CannotReadFile);
    EXPECT_TRUE(failure.GetError().GetArguments().empty());
}

TEST(ResultTests, PointerResultsDereferenceToThePointee)
{
    Counter counter;
    Core::Result<Counter*> raw = &counter;
    EXPECT_EQ(raw->Next(), 1);

    Core::Result<std::shared_ptr<Counter>> shared = std::make_shared<Counter>();
    EXPECT_EQ(shared->Next(), 1);
    EXPECT_EQ(shared->Next(), 2);

    Core::Result<Counter> owned = Counter{ 5 };
    EXPECT_EQ(owned->Next(), 6);
}
