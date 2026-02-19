#include "soundbank.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include "math.h"

static const double K0[] = { 0.0, 0.9375, 1.796875, 1.53125, 1.90625 };
static const double K1[] = { 0.0, 0.0, -0.8125, -0.859375, -0.9375 };




SoundBank::~SoundBank()
{
	for (auto& s : sounds) 
	{
        if (IsSoundValid(s))
        {
            if (IsSoundPlaying(s))
                StopSound(s);
            UnloadSound(s);
        }
            
	}
}

bool SoundBank::Load(const std::vector<uint8_t>& vhData, const std::vector<uint8_t>& vbData)
{
    // 1. Очистка без риска краша
    UnloadAll();

    if (vhData.size() < 32) return false;

    // 1. Читаем заголовок (Offsets по спецификации PS1)
    uint16_t programCount = *reinterpret_cast<const uint16_t*>(vhData.data() + 18);
    uint16_t vagCount = *reinterpret_cast<const uint16_t*>(vhData.data() + 22);

    // 2. ВАЖНО: Если программа одна, PS1 все равно может резервировать минимум
    if (programCount == 0) return false;

    // 3. РАСЧЕТ АДРЕСА ТАБЛИЦЫ (Формула из вашего кода IDA)
    // 2080 (Start of Tones) + (programCount * 512)
    size_t tableAddr = 2080 + (static_cast<size_t>(programCount) * 512);

    // Добавляем +2 байта (пропуск магического числа таблицы/выравнивание)
    tableAddr += 2;

    TraceLog(LOG_INFO, "VAB: progs=%d, vags=%d, table_addr=0x%zX", programCount, vagCount, tableAddr);

    // Проверка на выход за границы файла VH
    if (tableAddr + (vagCount * 2) > vhData.size()) {
        TraceLog(LOG_WARNING, "VAB: Table address 0x%zX is out of VH bounds (size %zu)", tableAddr, vhData.size());
        return false;
    }

    const uint16_t* sizeTable = reinterpret_cast<const uint16_t*>(vhData.data() + tableAddr);

    uint32_t currentOffset = 0;
    for (int i = 0; i < vagCount; ++i)
    {
        // Размер в блоках по 8 байт
        uint32_t vagSize = static_cast<uint32_t>(sizeTable[i]) * 8;

        if (vagSize == 0) continue;
        if (currentOffset + vagSize > vbData.size()) break;

        // Декодируем
        std::vector<int16_t> pcm = DecodeADPCM(vbData.data() + currentOffset, vagSize);

        if (!pcm.empty()) {
            Wave wave = { 0 };
            wave.frameCount = (unsigned int)pcm.size();

            // ЧАСТОТА: Если звуки слишком быстрые, попробуйте 11025. 
            // Если слишком медленные или низкие - 22050.
            wave.sampleRate = 22050;
            wave.sampleSize = 16;
            wave.channels = 1;
            wave.data = malloc(pcm.size() * sizeof(int16_t));

            if (wave.data) {
                memcpy(wave.data, pcm.data(), pcm.size() * sizeof(int16_t));
                Sound sound = LoadSoundFromWave(wave);
                UnloadWave(wave);
                if (IsSoundReady(sound)) sounds.push_back(sound);
            }
        }
        currentOffset += vagSize;
    }

    return !sounds.empty();
}

void SoundBank::UnloadAll()
{
    for (auto& s : sounds)
    {
        if (IsSoundValid(s))
        {
            if (IsSoundPlaying(s))
                StopSound(s);
            UnloadSound(s);
        }
            
    }
    sounds.clear();
}

void SoundBank::Play(int index)
{
    if (index >= 0 && index < sounds.size()) 
    {
        if (IsSoundValid(sounds[index]))
        {
            // Небольшая рандомизация питча, чтобы звучало живее (как в игре)
            SetSoundPitch(sounds[index], 1.0f);
            PlaySound(sounds[index]);
        }
        
        
    }
}

void SoundBank::Stop(int index)
{
    if (index >= 0 && index < sounds.size()) 
    {
        if (IsSoundValid(sounds[index]))
            StopSound(sounds[index]);
    }

}

void SoundBank::StopAll()
{
    for (auto& s : sounds)
    {
        if (IsSoundValid(s)) 
            StopSound(s);
    }
}

bool SoundBank::IsPlayering(int index)
{
    if (index >= 0 && index < sounds.size())
        return IsSoundValid(sounds[index]) && IsSoundPlaying(sounds[index]);
    else
        return false;
}

bool SoundBank::IsSoundReady(Sound s)
{
    return IsSoundValid(s);
}

Sound SoundBank::GetSound(int index)
{
    if (index >= 0 && index < sounds.size()) 
    {
        if(IsSoundValid(sounds[index]))
            return sounds[index];
    }
    return { 0 };
}

std::vector<int16_t> SoundBank::DecodeADPCM(const uint8_t* src, size_t size)
{
    std::vector<int16_t> buffer;
    buffer.reserve(size * 4); // Резервируем место

    double s_1 = 0.0; // Предыдущий сэмпл
    double s_2 = 0.0; // Пред-предыдущий сэмпл

    // Читаем блоками по 16 байт
    for (size_t i = 0; i < size; i += 16)
    {
        // Защита от выхода за пределы массива
        if (i + 16 > size) break;

        const uint8_t* block = src + i;

        // Байт 0: Сдвиг и индекс фильтра
        int shift = block[0] & 0x0F;
        int filter = (block[0] >> 4) & 0x0F;

        // Байт 1: Флаги
        uint8_t flags = block[1];

        // Валидация фильтра (всего 5, если больше - мусор)
        if (filter >= 5) filter = 0;

        // Декодируем 28 сэмплов (14 байт данных)
        for (int j = 2; j < 16; ++j)
        {
            uint8_t byte = block[j];

            // Обработка двух половин байта (nibbles)
            for (int k = 0; k < 2; ++k) {
                int8_t nibble = (k == 0) ? (byte & 0x0F) : (byte >> 4);

                // Расширение знака 4-бит -> 8-бит
                if (nibble & 0x08) nibble |= 0xF0;

                double sample = (double)(nibble << (12 - shift));
                double output = sample + s_1 * K0[filter] + s_2 * K1[filter];

                s_2 = s_1;
                s_1 = output;

                buffer.push_back(MATH::Clamp16((int32_t)output));
            }
        }

        // === ВАЖНОЕ ИСПРАВЛЕНИЕ ===
        // Проверяем флаг конца (Bit 0)
        // Если флаг == 1 (Loop End) или 3 (Loop End + Loop Start), мы должны остановиться,
        // если это не зацикленный звук. Для простого плеера лучше останавливаться всегда.
        if ((flags & 1) != 0) {
            break; // Звук закончился, выходим из цикла
        }
    }

    return buffer;
}
