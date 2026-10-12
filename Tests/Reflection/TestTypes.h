#pragma once

#include "Core/Guid.h"
#include "Math/Color.h"
#include "Math/Quaternion.h"
#include "Reflection/Object.h"

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

    BrightClass(Abstract)
    class TestBase : public Reflection::Object
    {
        BrightBody()
    public:
        int GetBaseValue() const { return m_baseValue; }

    private:
        BrightProperty(EditAnywhere, Category = "Base")
        int m_baseValue = 7;
    };

    BrightClass(Category = "Testing")
    class TestDerived : public TestBase
    {
        BrightBody()
    public:
        float speed() const { return m_moveSpeed; }
        void SetHealth(int health) { m_health = health; }
        int GetHealth() const { return m_health; }

        BrightProperty(EditDefaultsOnly, Category = "Movement")
        float m_moveSpeed = 600.0f;

        BrightProperty(VisibleAnywhere)
        BrightUIProperty(Read)
        int m_health = 100;

        BrightUIProperty(Write)
        std::string m_playerName = "Player";

        BrightProperty(EditInstanceOnly)
        TestMode m_mode = TestMode::Walking;

        BrightProperty(EditAnywhere)
        Math::Vector3 m_offset{ 1.0f, 2.0f, 3.0f };

        BrightProperty(EditAnywhere)
        Math::Quaternion m_rotation;

        BrightProperty(EditAnywhere)
        Math::Color m_tint{ 0.5f, 0.25f, 1.0f, 1.0f };

        BrightProperty(Hidden)
        bool m_flag = false;

        BrightProperty(EditAnywhere)
        std::vector<float> m_weights = { 1.0f, 2.0f };

        BrightProperty(EditAnywhere)
        TestReference m_reference;

        BrightProperty(EditAnywhere)
        std::vector<TestReference> m_references;

        int m_notReflected = 3;
    };
}
