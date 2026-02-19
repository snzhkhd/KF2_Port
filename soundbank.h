#pragma once
#include <vector>
#include <cstdint>
#include "PsxAudio.h"
#include "types.h"


struct SeqSlot {
    bool active = false;
    const uint8_t* pData = nullptr; // Указатель на начало данных MIDI
    uint32_t dataSize = 0;
    uint32_t currentPos = 0;        // Текущий байт в файле SEQ

    // Тайминги
    uint32_t waitTicks = 0;         // Сколько тиков ждем до следующего события
    float    tickAccumulator = 0.0f; // Накопитель времени (дробные тики)

    // Параметры из заголовка
    uint16_t resolution = 0;        // TPQN (из файла)
    uint32_t tempo = 500000;        // Темп (из файла)
    uint16_t vabID = 0;             // Какую библиотеку звуков используем
    uint8_t runningStatus = 0;
    struct Channel {
        uint8_t volume = 127;
        uint8_t pan = 64;
        uint8_t program = 0;        // Текущий инструмент (номер звука в VAB)
    } channels[16];
};


struct VabPair {
    int vh = -1;
    int vb = -1;
};
struct MusicMap
{
    int SeqId = 112;
    VabPair pair = {2,3};
};


class SeqPlayer 
{
public:
    SeqPlayer();

    bool Load(const ByteArray& data, int slot);

    void Update(float deltaTime, class AudioSystem* system);
    int  Play(const ByteArray& data, uint16_t vabID);

    void StopAll();

    int FindFreeSlot();

    void SetVolume(int slotIdx, uint8_t left, uint8_t right);
private:
    SeqSlot slots[16];
    bool ParseNextEvent(SeqSlot& s, class AudioSystem* system);
    static uint32_t ReadVLQ(const uint8_t* data, uint32_t& pos, uint32_t size);
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    // Загрузить пару файлов (VH - заголовок, VB - данные)
    bool Load(const ByteArray& vhData, const ByteArray& vbData);

    void UnloadAll();

    bool LoadVab(const ByteArray& vhData, const ByteArray& vbData);
    


    void PlaySfx(int index, float volume = 1.0f, float pitch = 1.0f);
    int PlayMusic(const ByteArray& seqData);
    void Update();
    // Вспомогательный метод для плеера SEQ
    void PlaySample(int program, float note, float volume, int channel);
    bool IsProgramReady(int program);

    void Play(int index);
    void Stop(int index);
    void StopAll();
    bool IsPlayering(int index);

    void NoteOff(int prog, int note);
    bool IsSoundReady(Sound s);
    // Получить звук для Raylib
    Sound GetSound(int index);
    size_t GetSoundCount() const { return sounds.size(); }


    SeqPlayer seqPlayer;

    MusicMap music[9];

    void SetPitchBend(int channel, float bend);
private:

    PsxSpu spu;

    Program programs[128];
    float channelBends[16];
    std::vector<Sound> sounds; // Готовые звуки Raylib

    // Декодер Sony ADPCM -> PCM 16-bit
    static std::vector<int16_t> DecodeADPCM(const uint8_t* src, size_t size);
};
