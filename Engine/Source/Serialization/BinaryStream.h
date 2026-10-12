#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace BrightBox3D::Serialization
{
    class BinaryWriter
    {
    public:
        template <typename T>
        void Write(const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            WriteBytes(&value, sizeof(T));
        }

        void WriteBytes(const void* data, size_t size)
        {
            const auto* bytes = static_cast<const uint8_t*>(data);
            m_buffer.insert(m_buffer.end(), bytes, bytes + size);
        }

        void WriteVarUInt(uint64_t value)
        {
            while (value >= 0x80)
            {
                m_buffer.push_back(static_cast<uint8_t>(value | 0x80));
                value >>= 7;
            }
            m_buffer.push_back(static_cast<uint8_t>(value));
        }

        void WriteString(const std::string& text)
        {
            WriteVarUInt(text.size());
            WriteBytes(text.data(), text.size());
        }

        template <typename T>
        void WriteVector(const std::vector<T>& values)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            WriteVarUInt(values.size());
            WriteBytes(values.data(), values.size() * sizeof(T));
        }

        std::vector<uint8_t>& GetBuffer() { return m_buffer; }
        const std::vector<uint8_t>& GetBuffer() const { return m_buffer; }

    private:
        std::vector<uint8_t> m_buffer;
    };

    class BinaryReader
    {
    public:
        BinaryReader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

        template <typename T>
        bool Read(T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            return ReadBytes(&value, sizeof(T));
        }

        bool ReadBytes(void* destination, size_t size)
        {
            if (m_offset + size > m_size)
            {
                m_failed = true;
                return false;
            }
            std::memcpy(destination, m_data + m_offset, size);
            m_offset += size;
            return true;
        }

        bool ReadVarUInt(uint64_t& value)
        {
            value = 0;
            int shift = 0;
            while (m_offset < m_size && shift < 64)
            {
                uint8_t byte = m_data[m_offset++];
                value |= static_cast<uint64_t>(byte & 0x7F) << shift;
                if ((byte & 0x80) == 0)
                {
                    return true;
                }
                shift += 7;
            }
            m_failed = true;
            return false;
        }

        bool ReadString(std::string& text)
        {
            uint64_t length = 0;
            if (!ReadVarUInt(length) || m_offset + length > m_size)
            {
                m_failed = true;
                return false;
            }
            text.assign(reinterpret_cast<const char*>(m_data + m_offset), static_cast<size_t>(length));
            m_offset += static_cast<size_t>(length);
            return true;
        }

        template <typename T>
        bool ReadVector(std::vector<T>& values)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            uint64_t count = 0;
            if (!ReadVarUInt(count) || m_offset + count * sizeof(T) > m_size)
            {
                m_failed = true;
                return false;
            }
            values.resize(static_cast<size_t>(count));
            return ReadBytes(values.data(), static_cast<size_t>(count) * sizeof(T));
        }

        const uint8_t* Current() const { return m_data + m_offset; }
        size_t Remaining() const { return m_size - m_offset; }
        bool Failed() const { return m_failed; }

    private:
        const uint8_t* m_data = nullptr;
        size_t m_size = 0;
        size_t m_offset = 0;
        bool m_failed = false;
    };
}
