#include "Core/Guid.h"

#include <mutex>
#include <random>

namespace BrightBox3D::Core
{
    namespace
    {
        int HexValue(char c)
        {
            if (c >= '0' && c <= '9')
            {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f')
            {
                return c - 'a' + 10;
            }
            if (c >= 'A' && c <= 'F')
            {
                return c - 'A' + 10;
            }
            return -1;
        }
    }

    Guid Guid::Generate()
    {
        static std::mutex mutex;
        static std::mt19937_64 engine([]
        {
            std::random_device device;
            std::seed_seq seed{ device(), device(), device(), device(), device(), device(), device(), device() };
            return std::mt19937_64(seed);
        }());

        std::lock_guard<std::mutex> lock(mutex);
        Guid guid;
        do
        {
            guid.high = engine();
            guid.low = engine();
        } while (!guid.IsValid());
        return guid;
    }

    Guid Guid::Parse(const std::string& text)
    {
        Guid guid;
        if (text.size() != 32)
        {
            return guid;
        }
        for (size_t i = 0; i < 32; ++i)
        {
            int value = HexValue(text[i]);
            if (value < 0)
            {
                return Guid{};
            }
            uint64_t& target = i < 16 ? guid.high : guid.low;
            target = (target << 4) | static_cast<uint64_t>(value);
        }
        return guid;
    }

    std::string Guid::ToString() const
    {
        static const char* digits = "0123456789abcdef";
        std::string text(32, '0');
        for (int i = 0; i < 16; ++i)
        {
            text[15 - i] = digits[(high >> (i * 4)) & 0xF];
            text[31 - i] = digits[(low >> (i * 4)) & 0xF];
        }
        return text;
    }
}
