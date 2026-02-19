#pragma once
#include <cstdint>
#include <array>
#include <vector>


namespace MATH
{
	float FixedToFloat(int32_t val) { return (float)val / 4096.0f; }
	int32_t FloatToFixed(float val) { return (int32_t)(val * 4096.0f); }


    static uint16_t readU16(const uint8_t* data, size_t& pos) {
        uint16_t val = data[pos] | (data[pos + 1] << 8);
        pos += 2;
        return val;
    }

    static uint32_t readU32(const uint8_t* data, size_t& pos) {
        uint32_t val = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24);
        pos += 4;
        return val;
    }

    static void writeU16(std::vector<uint8_t>& out, uint16_t val) {
        out.push_back(val & 0xFF);
        out.push_back((val >> 8) & 0xFF);
    }

    static void writeU32(std::vector<uint8_t>& out, uint32_t val) {
        out.push_back(val & 0xFF);
        out.push_back((val >> 8) & 0xFF);
        out.push_back((val >> 16) & 0xFF);
        out.push_back((val >> 24) & 0xFF);
    }

    static int16_t Clamp16(int32_t val) {
        if (val > 32767) return 32767;
        if (val < -32768) return -32768;
        return static_cast<int16_t>(val);
    }

    template<typename T>
    struct PSXVectorTemplate
    {
        T vx;
        T vy;
        T vz;
        T pad;
    };

    using SVECTOR = PSXVectorTemplate<int16_t>;
    using VECTOR = PSXVectorTemplate<int32_t>;


    template<typename in = int32_t>
    inline constexpr float fromAngle(in x)
    {
        return x * FixedToFloat(360);
    }

    template<typename out = int32_t>
    inline constexpr out toAngle(float x)
    {
        return static_cast<out>(x / FixedToFloat(360));
    }
}