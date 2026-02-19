#pragma once
#include "types.h" //ByteArray
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <numeric>


namespace Utilities
{

    // Хелпер для сравнения "магических чисел" (вместо QByteArray::fromHex)
    inline bool matchMagic(const ByteArray& data, size_t offset, const std::vector<uint8_t>& magic)
    {
        if (offset + magic.size() > data.size()) return false;
        return std::memcmp(data.data() + offset, magic.data(), magic.size()) == 0;
    }

    // --- Преобразование типов данных (as<T>) ---

    template<class T>
    T& as(ByteArray& array, size_t offset = 0)
    {
        return *reinterpret_cast<T*>(array.data() + offset);
    }

    template<class T>
    const T& as(const ByteArray& array, size_t offset = 0)
    {
        return *reinterpret_cast<const T*>(array.data() + offset);
    }

    template<class T>
    const T& as(const uint8_t* ptr, size_t offset = 0)
    {
        return *reinterpret_cast<const T*>(ptr + offset);
    }

    // --- Clamping (Ограничение значений) ---

    template<class T>
    uint8_t clampToByte(T value)
    {
        return static_cast<uint8_t>(std::clamp<T>(value, 0, 0xFF));
    }

    template<class T>
    int16_t clampToShort(T value)
    {
        return static_cast<int16_t>(std::clamp<T>(value, -32767, 32767));
    }

    template<class T>
    uint16_t clampToUShort(T value)
    {
        return static_cast<uint16_t>(std::clamp<T>(value, 0, 0xFFFF));
    }

    // --- Детекторы типов файлов ---

    inline bool fileIsGameDB(const ByteArray& file)
    {
        // 40 10 FF 00 00 00
        if (!matchMagic(file, 4, { 0x40, 0x10, 0xFF, 0x00, 0x00, 0x00 })) return false;
        // 00 00 00 00 00 00 00 00 40 10 FF 00 00 00
        if (!matchMagic(file, 20, { 0,0,0,0,0,0,0,0, 0x40, 0x10, 0xFF, 0x00, 0x00, 0x00 })) return false;
        return true;
    }

    inline bool fileIsMAP1(const ByteArray& file)
    {
        return matchMagic(file, 0, { 0x00, 0xFA, 0x00, 0x00 });
    }

    inline bool fileIsMAP2(const ByteArray& file)
    {
        return matchMagic(file, 0, { 0xC0, 0x32, 0x00, 0x00 });
    }

    inline bool fileIsMAP3(const ByteArray& file)
    {
        if (file.size() < 0x20) return false;
        return file[0x03] == 0x80 && file[0x07] == file[0x0b]
            && file[0x0b] == file[0x0f] && file[0x13] == file[0x17];
    }

    inline bool fileIsMO(const ByteArray& file)
    {
        if (file.size() < 12) return false;
        uint32_t tmdOff = as<uint32_t>(file, 8);
        return (tmdOff < file.size()) && (file[tmdOff] == 0x41);
    }

    inline bool fileIsPSXEXE(const ByteArray& file)
    {
        if (file.size() < 8) return false;
        return std::memcmp(file.data(), "PS-X EXE", 8) == 0;
    }

    inline bool fileIsTIM(const ByteArray& file)
    {
        if (file.size() < 8) return false;
        return matchMagic(file, 0, { 0x10, 0x00, 0x00, 0x00 }) && (file[4] > 0 && file[4] <= 15);
    }

    inline bool fileIsTMD(const ByteArray& file)
    {
        return matchMagic(file, 0, { 0x41, 0x00, 0x00, 0x00 });
    }

    /*!
      * \brief Проверяет, является ли файл VB (VAB Body - сэмплы звука).
      * Проверяет, что первые 16 байт файла заполнены нулями.
      */
    inline bool fileIsVB(const ByteArray& file)
    {
        if (file.size() < 16) return false;

        // Создаем массив из 16 нулей для сравнения
        static const uint8_t zeroBlock[16] = { 0 };

        // Сравниваем начало файла с блоком нулей
        return std::memcmp(file.data(), zeroBlock, 16) == 0;
    }

    /*!
     * \brief Проверяет, является ли файл RTIM.
     * Логика: байты [8-15] должны быть равны байтам [0-7],
     * при этом байты [4-7] НЕ должны быть равны байтам [0-3].
     */
    inline bool fileIsRTIM(const ByteArray& file)
    {
        if (file.size() < 16) return false;

        // 1. Сравниваем блок [8..15] с блоком [0..7]
        bool firstBlockMatch = std::memcmp(file.data() + 8, file.data(), 8) == 0;

        // 2. Сравниваем блок [4..7] с блоком [0..3] (должны НЕ совпадать)
        bool secondBlockMismatch = std::memcmp(file.data() + 4, file.data(), 4) != 0;

        return firstBlockMatch && secondBlockMismatch;
    }

    inline bool fileIsVH(const ByteArray& file)
    {
        return matchMagic(file, 0, { 0x70, 0x42, 0x41, 0x56 }); // "VABp"
    }

    inline bool fileIsSEQ(const ByteArray& file)
    {
        return matchMagic(file, 0, { 0x70, 0x51, 0x45, 0x53 }); // "SEQp"
    }

    inline bool fileIsRTMD(const ByteArray& file)
    {
        if (!matchMagic(file, 0, { 0, 0, 0, 0 })) return false;
        return matchMagic(file, 4, { 0x12, 0, 0, 0 }) || matchMagic(file, 4, { 0x10, 0, 0, 0 });
    }

    inline bool fileIsSTTMD(const ByteArray& file)
    {
        if (!fileIsTMD(file) || file.size() < 0x24 + 16) return false;

        uint16_t tmdPrimCount = as<uint16_t>(file, 0x20);
        uint16_t stPrimCounts[8];
        std::memcpy(stPrimCounts, file.data() + 0x24, 16);

        uint32_t sum = 0;
        for (int i = 0; i < 8; ++i) sum += stPrimCounts[i];

        return tmdPrimCount == sum;
    }
}