#pragma once
#include <vector>
#include <cstdint>
#include <raylib.h>

// Стандартный заголовок VAB (из вашего примера)
struct VABHeader
{
    uint32_t magic;         // "pBAV"
    uint32_t version;
    uint32_t vabId;
    uint32_t fileSize;      // Общий размер (часто неточный в KF)
    uint16_t reserved0;
    uint16_t programCount;
    uint16_t toneCount;
    uint16_t vagCount;      // КОЛИЧЕСТВО СЭМПЛОВ
    uint8_t masterVolume;
    uint8_t masterPan;
    uint8_t bankAttr1;
    uint8_t bankAttr2;
    uint32_t reserved1;
    // Дальше идут ProgramAttributes и ToneAttributes, 
    // но для извлечения звука нам нужны только VAG Offsets в конце файла.
};

class SoundBank {
public:
    SoundBank() = default;
    ~SoundBank();

    // Загрузить пару файлов (VH - заголовок, VB - данные)
    bool Load(const std::vector<uint8_t>& vhData, const std::vector<uint8_t>& vbData);

    void UnloadAll();

    void Play(int index);
    void Stop(int index);
    void StopAll();
    bool IsPlayering(int index);

    bool IsSoundReady(Sound s);
    // Получить звук для Raylib
    Sound GetSound(int index);
    size_t GetSoundCount() const { return sounds.size(); }

private:
    std::vector<Sound> sounds; // Готовые звуки Raylib

    // Декодер Sony ADPCM -> PCM 16-bit
    static std::vector<int16_t> DecodeADPCM(const uint8_t* src, size_t size);
};