#pragma once
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <map>

const int SPU_VOICES_COUNT = 127;//127;
const int SFX_VOICE_LIMIT = 128;//128; // 0..19 для эффектов, 20..23 для музыки/лупов

const int SPU_SAMPLE_RATE = 44100;
const int MAX_AUDIO_BUFFER = 8192;

enum AdsrState { ADSR_IDLE, ADSR_ATTACK, ADSR_DECAY, ADSR_SUSTAIN, ADSR_RELEASE };
enum WaveType { WAVE_SINE, WAVE_SAW, WAVE_SQUARE, WAVE_NOISE };


enum class InstrumentType { Synth, Sample };

struct AdsrSettings 
{
    float attack = 0.001f;
    float decay = 0.5f;
    float sustain = 0.5f;
    float release = 1.0f;
};


struct Instrument {
    //float* data = nullptr;
    std::vector<float> data;
    unsigned int sampleCount = 0;
    bool loop = false;

    int baseNote;       // Базовая нота (обычно 60 - Middle C) для правильного питча
    InstrumentType type;
    int8_t fineTune = 0;
    AdsrSettings adsr;
};

struct Tone {
    std::vector<float> data;
    uint32_t sampleCount = 0;
    uint8_t minNote = 0;
    uint8_t maxNote = 127;
    uint8_t centerNote = 60;
    uint8_t vol = 127;
    int8_t fineTune = 0;
    // ADSR
    float attack = 0.001f;
    float decay = 0.5f;
    float sustain = 0.5f;
    float release = 1.0f;

    InstrumentType type = InstrumentType::Sample;
    bool loop = false;
};

struct Program 
{
    Tone tones[16]; // Фиксированный массив (указатели на элементы будут вечными)
    int toneCount = 0;
};

class SpuVoice {
private:
    // КУБИЧЕСКАЯ ИНТЕРПОЛЯЦИЯ (Эмуляция "мягкого" звука PS1)
    float CubicInterp(float y0, float y1, float y2, float y3, float mu) {
        float a0, a1, a2, a3;
        float mu2 = mu * mu;
        a0 = y3 - y2 - y0 + y1;
        a1 = y0 - y1 - a0;
        a2 = y2 - y0;
        a3 = y1;
        return (a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3);
    }

public:
    bool active = false;
    float pitch = 1.0f;
    float sampleCursor = 0.0f;
    float leftVolume = 1.0f, rightVolume = 1.0f;
    uint8_t currentNote = 0;
    int parentProgramID = -1;
    float basePitch = 1.0f;
    AdsrSettings adsr;
    AdsrState state = ADSR_IDLE;
    float currentEnvelopeVal = 0.0f;

    const Tone* instrument = nullptr;

    // Храним последний сэмпл для FM-модуляции следующего канала
    float lastSampleOutput = 0.0f;

    void NoteOn(const Tone* inst, float p, float vol,float pan, int note, int progID)
    {
        instrument = inst;
        basePitch = p;
        pitch = p;
        currentNote = (uint8_t)note;
        parentProgramID = progID; // Запоминаем программу
        active = true;
        state = ADSR_ATTACK;
        sampleCursor = 0.0f;
        currentEnvelopeVal = 0.0f;

        // Расчет стерео (pan: 0.0 - лево, 0.5 - центр, 1.0 - право)
        leftVolume = cosf(pan * (float)PI / 2.0f) * vol;
        rightVolume = sinf(pan * (float)PI / 2.0f) * vol;

        adsr.attack = inst->attack;
        adsr.sustain = inst->sustain;
        adsr.decay = inst->decay;
        adsr.release = inst->release;
        
    }

    void NoteOff()
    {
        if (state != ADSR_IDLE)
            state = ADSR_RELEASE;
    }

    

    bool IsNotePlaying()
    {
        return active;// && (state != ADSR_IDLE);
    }
    void SetVolumeAndPan(float vol, float pan) {
        // Та же логика панорамы, что и в NoteOn
        leftVolume = cosf(pan * PI / 2.0f) * vol;
        rightVolume = sinf(pan * PI / 2.0f) * vol;
        // volume само по себе у нас хранит "начальную" громкость, 
        // но для динамического 3D лучше обновлять left/right напрямую.
    }

    // modInput - это сигнал с предыдущего канала для FM-синтеза
    void GetSamples(float* outL, float* outR, float dt, float modInput = 0.0f)
    {
        // Если голос выключен или ADSR закончился
        if (!active || state == ADSR_IDLE) {
            *outL = 0; *outR = 0;
            lastSampleOutput = 0.0f;
            return;
        }

        // === 1. ОБНОВЛЕНИЕ ADSR (Твой код без изменений) ===
        float rate = 0.0f;
        float target = 0.0f;

        switch (state) {
        case ADSR_ATTACK:
            currentEnvelopeVal += dt / (adsr.attack + 0.001f);
            if (currentEnvelopeVal >= 1.0f) { currentEnvelopeVal = 1.0f; state = ADSR_DECAY; }
            break;
        case ADSR_DECAY:
            target = adsr.sustain;
            rate = 5.0f / (adsr.decay + 0.001f);
            currentEnvelopeVal += (target - currentEnvelopeVal) * rate * dt;
            if (fabs(currentEnvelopeVal - target) < 0.01f) {
                currentEnvelopeVal = target;
                state = ADSR_SUSTAIN;
            }
            break;
        case ADSR_SUSTAIN:
            currentEnvelopeVal += (adsr.sustain - currentEnvelopeVal) * 10.0f * dt;
            break;
        case ADSR_RELEASE:
            target = 0.0f;
            rate = 1.0f / (adsr.release + 0.001f);
            //rate = 1.0f / (adsr.release + 0.001f);
            currentEnvelopeVal += (target - currentEnvelopeVal) * rate * dt;
            if (currentEnvelopeVal < 0.001f) {
                currentEnvelopeVal = 0.0f;
                state = ADSR_IDLE;
                active = false;
            }
            break;
        default: break;
        }

        float raw = 0.0f;

        if (instrument && !instrument->data.empty()) {
            unsigned int len = instrument->sampleCount;

            // === РАЗДЕЛЕНИЕ ЛОГИКИ ДЛЯ СЭМПЛОВ И СИНТЕЗАТОРА ===

            float step = 0.0f;
            bool isSample = (instrument->type == InstrumentType::Sample);

            if (isSample) {
                // --- ЛОГИКА ДЛЯ WAV (СЭМПЛЫ) ---

                // 1. Скорость просто равна питчу (1.0 = нормальная скорость)
                // 44100.0f нужно, так как dt у нас в секундах, а курсор в сэмплах
                // Если pitch = 1.0, мы двигаемся на 44100 сэмплов в секунду
                step = pitch *(SPU_SAMPLE_RATE * dt);
                
                // FM модуляция для сэмплов (обычно не нужна, но можно оставить для эффектов)
                float fmFactor = 1.0f + (modInput * 0.1f);
                step *= fmFactor;

                // 2. Движение курсора
                sampleCursor += step;

                // 3. Обработка конца файла
                if (sampleCursor >= len) {
                    if (instrument->loop) {
                        sampleCursor = fmod(sampleCursor, (float)len);
                    }
                    else {
                        active = false;
                        *outL = 0; *outR = 0;
                        return;
                    }
                }
            }
            else {
                // --- ЛОГИКА ДЛЯ СИНТЕЗАТОРА (WAVETABLE) ---

                float fmFactor = 1.0f + (modInput * 0.5f);
                float targetFreq = 261.63f * pitch * fmFactor;
                step = ((float)len * targetFreq) * dt; // dt уже есть (1/SampleRate)
                // Или твоя старая формула:
                // step = ((float)len * targetFreq) / (float)SPU_SAMPLE_RATE; 

                sampleCursor += step;

                // Синтезатор всегда зациклен (волна)
                if (sampleCursor >= len) sampleCursor -= len;
                if (sampleCursor < 0) sampleCursor += len;
            }

            // === ИНТЕРПОЛЯЦИЯ ===

            int i1 = (int)sampleCursor;
            int i2 = i1 + 1;
            int i0 = i1 - 1;
            int i3 = i1 + 2;

            // Безопасное чтение индексов
            if (isSample) {
                // Для сэмплов не крутим индексы по кругу (кроме случая loop, но он выше обработан)
                // Просто клампим края, чтобы не вылететь за массив
                // i0 (предыдущий) всегда клампим или враппим если надо (но для кубики кламп безопаснее)
                if (i0 < 0) {
                    // Если зациклено - берем с конца, иначе 0
                    i0 = instrument->loop ? (len - 1) : 0;
                }

                // i1 (текущий)
                if (i1 >= len) i1 = len - 1;

                if (instrument->loop) {
                    // ПРАВИЛЬНОЕ ЗАЦИКЛИВАНИЕ ДЛЯ КУБИКИ
                    if (i0 < 0) i0 = len - 1;
                    i1 %= len;
                    i2 %= len;
                    i3 %= len;
                }
                else {
                    // КЛАМПИНГ ДЛЯ ОДИНОЧНЫХ ЗВУКОВ
                    if (i0 < 0) i0 = 0;
                    if (i1 >= len) i1 = len - 1;
                    if (i2 >= len) i2 = len - 1;
                    if (i3 >= len) i3 = len - 1;
                }
                 //i2 (следующий) и i3 (через один)
                //if (instrument->loop) {
                //    // ЕСЛИ ЗАЦИКЛЕНО: берем данные из НАЧАЛА массива
                //    if (i2 >= len) i2 %= len;
                //    if (i3 >= len) i3 %= len;
                //}
                //else {
                //    // ЕСЛИ НЕ ЗАЦИКЛЕНО: удерживаем последний сэмпл
                //    if (i2 >= len) i2 = len - 1;
                //    if (i3 >= len) i3 = len - 1;
                //}
            }
            else {
                // Для синтезатора крутим по кругу
                if (i1 >= len) i1 %= len;
                if (i2 >= len) i2 %= len;
                if (i0 < 0) i0 += len;
                if (i3 >= len) i3 %= len;
            }

            // ЧТЕНИЕ ДАННЫХ
            // ВАЖНО: Убираем деление на 32767 для сэмплов (они уже float)
            // Для синтезатора оставляем деление, ЕСЛИ он все еще генерирует большие числа (short)

            float y0, y1, y2, y3;

            if (isSample) {
                // WAV (Float -1..1) - читаем как есть
                y0 = instrument->data[i0];
                y1 = instrument->data[i1];
                y2 = instrument->data[i2];
                y3 = instrument->data[i3];
            }
            else {
                // Synth (Short -32k..32k) - делим
                // (Предполагаю, что старый генератор делает short, даже если массив float*)
                y0 = instrument->data[i0] / 32767.0f;
                y1 = instrument->data[i1] / 32767.0f;
                y2 = instrument->data[i2] / 32767.0f;
                y3 = instrument->data[i3] / 32767.0f;
            }

            float frac = sampleCursor - (int)sampleCursor;
            raw = CubicInterp(y0, y1, y2, y3, frac);
        }

        lastSampleOutput = raw * currentEnvelopeVal;

        *outL = lastSampleOutput * leftVolume;
        *outR = lastSampleOutput * rightVolume;
    }
};




class PsxSpu {
private:
    short pcmBuffer[MAX_AUDIO_BUFFER * 2];
public:

    
    int sfxCursor = 0;

    SpuVoice voices[SPU_VOICES_COUNT];
    AudioStream stream;
    float* reverbBuffer;
    int reverbCursor = 0, reverbSize = 0;
    float masterVolume = 0.75f;
    float reverbMix = 0.01f;
    float reverbDecay = 0.01f; // PS1 reverb was quite long
    int currentBufferSize = 1024;//1024;
    int currentVoiceIndex = 0;

    // Массив флагов: включен ли FM для голоса N (модулируется ли он голосом N-1)
    bool voiceFmFlags[SPU_VOICES_COUNT] = { false };

    PsxSpu() {
        InitAudioSystem(1024);
        reverbSize = (int)(SPU_SAMPLE_RATE * 0.35f);
        reverbBuffer = (float*)calloc(reverbSize * 2, sizeof(float));
        memset(pcmBuffer, 0, sizeof(pcmBuffer));
    }

    ~PsxSpu()
    {
        if (IsAudioStreamValid(stream))
            UnloadAudioStream(stream);
        free(reverbBuffer);
    }

    void InitAudioSystem(int bufferSize) {
        if (IsAudioStreamValid(stream)) UnloadAudioStream(stream);
        SetAudioStreamBufferSizeDefault(bufferSize);
        stream = LoadAudioStream(SPU_SAMPLE_RATE, 16, 2);
        PlayAudioStream(stream);
        currentBufferSize = bufferSize;
    }

    void StopAllVoices()
    {
        for (int i = 0; i < SPU_VOICES_COUNT; i++) {
            voices[i].active = false;
            voices[i].state = ADSR_IDLE;
            voices[i].instrument = nullptr; // Очень важно обнулить указатель
        }
    }


    int PlayNote(const Tone* inst, float pitch, float volume, bool fmEnabled, float pan, int note, int progID)
    {
        int id = -1;
        for (int i = 0; i < SPU_VOICES_COUNT; i++) {
            if (!voices[i].active || voices[i].state == ADSR_IDLE) {
                id = i;
                break;
            }
        }
        // Если все 127 голосов заняты (что вряд ли), крадем самый старый
        if (id == -1) 
        {
            id = sfxCursor;
            sfxCursor = (sfxCursor + 1) % SPU_VOICES_COUNT;
            printf("all voice is bussy!\n");
        }

        // Запускаем ноту на выбранном ID
        voices[id].active = false; // Сброс перед NoteOn
        voices[id].NoteOn(inst, pitch, volume, pan, note, progID);
        voiceFmFlags[id] = fmEnabled;

        return id;
        //int id = -1;

        //// === СТРАТЕГИЯ ВЫБОРА ГОЛОСА ===

        //if (inst->loop) {
        //    // --- ДЛЯ ЛУПОВ (Музыка, гул ламп) ---
        //    // Ищем ПЕРВЫЙ СВОБОДНЫЙ слот в "безопасной зоне" (20-23)
        //    for (int i = SFX_VOICE_LIMIT; i < SPU_VOICES_COUNT; i++) {
        //        if (!voices[i].active || voices[i].state == ADSR_IDLE) {
        //            id = i;
        //            break;
        //        }
        //    }
        //    // Если все резервные слоты заняты, пробуем перезаписать самый старый (или просто первый попавшийся в резерве)
        //    // Для простоты: если места нет, берем самый последний слот
        //    if (id == -1) id = SPU_VOICES_COUNT - 1;
        //}
        //else {
        //    // --- ДЛЯ SFX (Шаги, выстрелы) ---
        //    // Используем карусель ТОЛЬКО в диапазоне 0..19
        //    id = sfxCursor;
        //    sfxCursor++;
        //    //if (sfxCursor >= SFX_VOICE_LIMIT) sfxCursor = 0;
        //}
        //
        //// Запускаем ноту на выбранном ID
        //voices[id].NoteOn(inst, pitch, volume, pan, note, progID);
        //voiceFmFlags[id] = fmEnabled;

        //return id;
    }

    void StopVoice(int id) { if (id >= 0 && id < SPU_VOICES_COUNT) voices[id].NoteOff(); }

    bool IsSoundPlaying(int id) { if (id >= 0 && id < SPU_VOICES_COUNT) return voices[id].IsNotePlaying(); return -1; }

    void SetVoice3D(int voiceId, float volume, float pan) {
        if (voiceId >= 0 && voiceId < SPU_VOICES_COUNT) {
            if (voices[voiceId].active) {
                voices[voiceId].SetVolumeAndPan(volume, pan);
            }
        }
    }

    void UpdateVoicePitch(int vIdx, float bend) {
        if (vIdx >= 0 && vIdx < SPU_VOICES_COUNT && voices[vIdx].active) {
            // Стандартный диапазон бенда в PS1 — 2 полутона (2.0f)
            float bendRange = 2.0f;
            voices[vIdx].pitch = voices[vIdx].basePitch * powf(2.0f, (bend * bendRange) / 12.0f);
        }
    }

    void Update() {
        if (!IsAudioStreamProcessed(stream)) return;
        while (IsAudioStreamProcessed(stream)) {
            int frames = currentBufferSize;
            if (frames > MAX_AUDIO_BUFFER) frames = MAX_AUDIO_BUFFER;
            float dt = 1.0f / SPU_SAMPLE_RATE;
            for (int i = 0; i < frames; i++) {
                float mL = 0, mR = 0;

                // Проходим по голосам
                for (int v = 0; v < SPU_VOICES_COUNT; v++) {
                    if (voices[v].active) {
                        float l, r;
                    //    printf("Voice %d is playing instrument %p\n", v, voices[v].instrument);
                        // Получаем вход для FM: выход ПРЕДЫДУЩЕГО голоса
                        // Если это 0-й голос, берем последний (23-й), или просто 0 (обычно 0)
                        float modIn = 0.0f;
                        int prevV = (v - 1);
                        if (prevV < 0) prevV = SPU_VOICES_COUNT - 1;

                        // Если FM включен для этого голоса, берем сэмпл соседа
                        if (voiceFmFlags[v] && voices[prevV].active) {
                            modIn = voices[prevV].lastSampleOutput;
                        }

                        voices[v].GetSamples(&l, &r, dt, modIn);
                        mL += l; mR += r;
                    }
                }

                float rL = reverbBuffer[reverbCursor * 2], rR = reverbBuffer[reverbCursor * 2 + 1];
                float fL = mL + rL * reverbMix, fR = mR + rR * reverbMix;
                reverbBuffer[reverbCursor * 2] = fL * reverbDecay;
                reverbBuffer[reverbCursor * 2 + 1] = fR * reverbDecay;
                reverbCursor = (reverbCursor + 1) % reverbSize;
                fL = tanhf(fL * masterVolume); fR = tanhf(fR * masterVolume);
                pcmBuffer[i * 2] = (short)(fL * 32000); 
                pcmBuffer[i * 2 + 1] = (short)(fR * 32000);
               /* pcmBuffer[i * 2] = (short)(fL * 24000);
                pcmBuffer[i * 2 + 1] = (short)(fR * 24000);*/
            }
            UpdateAudioStream(stream, pcmBuffer, frames);
        }
    }
};