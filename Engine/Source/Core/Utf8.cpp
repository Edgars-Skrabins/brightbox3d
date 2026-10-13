#include "Core/Utf8.h"

namespace BrightBox3D::Core::Utf8
{
    namespace
    {
        constexpr char32_t OneByteLimit = 0x80;
        constexpr char32_t TwoByteLimit = 0x800;
        constexpr char32_t ThreeByteLimit = 0x10000;
        constexpr uint32_t ContinuationMask = 0xC0;
        constexpr uint32_t ContinuationPrefix = 0x80;
        constexpr uint32_t PayloadMask = 0x3F;
        constexpr uint32_t TwoBytePrefix = 0xC0;
        constexpr uint32_t ThreeBytePrefix = 0xE0;
        constexpr uint32_t FourBytePrefix = 0xF0;

        char Continuation(char32_t codepoint, int shift)
        {
            return static_cast<char>(ContinuationPrefix | ((codepoint >> shift) & PayloadMask));
        }

        uint32_t Payload(char byte)
        {
            return static_cast<uint32_t>(static_cast<unsigned char>(byte)) & PayloadMask;
        }
    }

    std::string Encode(char32_t codepoint)
    {
        std::string result;
        if (codepoint < OneByteLimit)
        {
            result += static_cast<char>(codepoint);
        }
        else if (codepoint < TwoByteLimit)
        {
            result += static_cast<char>(TwoBytePrefix | (codepoint >> 6));
            result += Continuation(codepoint, 0);
        }
        else if (codepoint < ThreeByteLimit)
        {
            result += static_cast<char>(ThreeBytePrefix | (codepoint >> 12));
            result += Continuation(codepoint, 6);
            result += Continuation(codepoint, 0);
        }
        else
        {
            result += static_cast<char>(FourBytePrefix | (codepoint >> 18));
            result += Continuation(codepoint, 12);
            result += Continuation(codepoint, 6);
            result += Continuation(codepoint, 0);
        }
        return result;
    }

    std::vector<uint32_t> Decode(std::string_view text)
    {
        std::vector<uint32_t> codepoints;
        for (size_t i = 0; i < text.size();)
        {
            uint32_t lead = static_cast<unsigned char>(text[i]);
            uint32_t codepoint = lead;
            size_t length = 1;
            if (lead >= FourBytePrefix && i + 3 < text.size())
            {
                codepoint = ((lead & 0x07) << 18) | (Payload(text[i + 1]) << 12) | (Payload(text[i + 2]) << 6) | Payload(text[i + 3]);
                length = 4;
            }
            else if (lead >= ThreeBytePrefix && i + 2 < text.size())
            {
                codepoint = ((lead & 0x0F) << 12) | (Payload(text[i + 1]) << 6) | Payload(text[i + 2]);
                length = 3;
            }
            else if (lead >= TwoBytePrefix && i + 1 < text.size())
            {
                codepoint = ((lead & 0x1F) << 6) | Payload(text[i + 1]);
                length = 2;
            }
            codepoints.push_back(codepoint);
            i += length;
        }
        return codepoints;
    }

    void PopCodepoint(std::string& text)
    {
        while (!text.empty())
        {
            uint32_t byte = static_cast<unsigned char>(text.back());
            text.pop_back();
            if ((byte & ContinuationMask) != ContinuationPrefix)
            {
                break;
            }
        }
    }
}
