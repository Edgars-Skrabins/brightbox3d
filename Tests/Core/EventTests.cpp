#include "Core/Event.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace BrightBox3D::Core;

TEST(EventTests, BroadcastReachesEverySubscriberInOrder)
{
    Event<int, const std::string&> event;
    std::vector<std::string> calls;
    event.Subscribe([&](int value, const std::string& text) { calls.push_back("first " + std::to_string(value) + text); });
    event.Subscribe([&](int value, const std::string& text) { calls.push_back("second " + std::to_string(value) + text); });

    event.Broadcast(7, "!");

    ASSERT_EQ(calls.size(), 2u);
    EXPECT_EQ(calls[0], "first 7!");
    EXPECT_EQ(calls[1], "second 7!");
}

TEST(EventTests, UnsubscribeRemovesOnlyThatListener)
{
    Event<> event;
    int first = 0;
    int second = 0;
    EventHandle firstHandle = event.Subscribe([&]() { ++first; });
    EventHandle secondHandle = event.Subscribe([&]() { ++second; });
    EXPECT_NE(firstHandle, secondHandle);

    event.Unsubscribe(firstHandle);
    event.Unsubscribe(firstHandle);
    event.Unsubscribe(secondHandle + 100);
    event.Broadcast();

    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
    EXPECT_FALSE(event.IsEmpty());
    event.Clear();
    EXPECT_TRUE(event.IsEmpty());
    event.Broadcast();
    EXPECT_EQ(second, 1);
}

TEST(EventTests, ChangesDuringBroadcastApplyFromTheNextBroadcast)
{
    Event<> event;
    int late = 0;
    int removed = 0;
    EventHandle removedHandle = 0;
    event.Subscribe([&]()
    {
        event.Unsubscribe(removedHandle);
        event.Subscribe([&]() { ++late; });
    });
    removedHandle = event.Subscribe([&]() { ++removed; });

    event.Broadcast();
    EXPECT_EQ(removed, 1);
    EXPECT_EQ(late, 0);

    event.Broadcast();
    EXPECT_EQ(removed, 1);
    EXPECT_EQ(late, 1);
}
