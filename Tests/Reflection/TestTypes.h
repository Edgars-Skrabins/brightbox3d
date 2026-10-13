#pragma once

#include "Core/Guid.h"
#include "Math/Color.h"
#include "Math/Quaternion.h"
#include "Reflection/EngineObject.h"
#include "TestCategories.h"

#include <string>
#include <vector>

namespace BrightBox3D::Tests
{
    BrightEnum()
    enum class TestMode : uint8_t
    {
        Idle,
        Walking = 5,
        Running
    };

    struct TestReference
    {
        static constexpr Reflection::PropertyKind ReflectionKind = Reflection::PropertyKind::AssetReference;
        static const char* ReflectionReferenceType() { return "TestAsset"; }

        Core::Guid guid;
    };

    BrightClass(.abstract = true)
    class TestBase : public Reflection::EngineObject
    {
        BrightBody()
    public:
        int GetBaseValue() const { return m_baseValue; }

    private:
        BrightProperty(.edit = Edit::Anywhere, .category = Base)
        int m_baseValue = 7;
    };

    BrightClass(.category = Testing)
    class TestDerived : public TestBase
    {
        BrightBody()
    public:
        float speed() const { return m_moveSpeed; }
        void SetHealth(int health) { m_health = health; }
        int GetHealth() const { return m_health; }

        BrightProperty(.edit = Edit::DefaultsOnly, .category = Movement)
        float m_moveSpeed = 600.0f;

        BrightProperty(.edit = Edit::Visible)
        BrightUIProperty(.access = UIAccess::Read)
        int m_health = 100;

        BrightUIProperty(.access = UIAccess::Write)
        std::string m_playerName = "Player";

        BrightProperty(.edit = Edit::InstanceOnly)
        TestMode m_mode = TestMode::Walking;

        BrightProperty(.edit = Edit::Anywhere)
        Math::Vector3 m_offset{ 1.0f, 2.0f, 3.0f };

        BrightProperty(.edit = Edit::Anywhere)
        Math::Quaternion m_rotation;

        BrightProperty(.edit = Edit::Anywhere)
        Math::Color m_tint{ 0.5f, 0.25f, 1.0f, 1.0f };

        BrightProperty(.edit = Edit::Hidden)
        bool m_flag = false;

        BrightProperty(.edit = Edit::Anywhere)
        std::vector<float> m_weights = { 1.0f, 2.0f };

        BrightProperty(.edit = Edit::Anywhere)
        TestReference m_reference;

        BrightProperty(.edit = Edit::Anywhere)
        std::vector<TestReference> m_references;

        int m_notReflected = 3;
    };
}
