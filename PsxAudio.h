#pragma once
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <map>

const int SPU_VOICES_COUNT = 127;
const int SFX_VOICE_LIMIT = 128;

const int SPU_SAMPLE_RATE = 44100;
const int MAX_AUDIO_BUFFER = 8192;

enum AdsrState { ADSR_IDLE, ADSR_ATTACK, ADSR_DECAY, ADSR_SUSTAIN, ADSR_RELEASE };
enum WaveType { WAVE_SINE, WAVE_SAW, WAVE_SQUARE, WAVE_NOISE };

double linearAmpDecayTimeToLinDBDecayTime(double secondsToFullAtten, double targetDb_LeastSquares = 70, double targetDb_InitialSlope = 140);
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
    float sustain = 1.0f;
    float release = 0.2f;

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
        if (!active || state == ADSR_IDLE) {
            *outL = 0; *outR = 0;
            lastSampleOutput = 0.0f;
            return;
        }

        // === ADSR ===
        float rate = 0.0f;
        float target = 0.0f;

        switch (state) {
        case ADSR_ATTACK:
            currentEnvelopeVal += dt / (adsr.attack + 0.001f);
            if (currentEnvelopeVal >= 1.0f) {
                currentEnvelopeVal = 1.0f;
                state = ADSR_DECAY;
            }
            break;
        case ADSR_DECAY:
            target = adsr.sustain;
            rate = 15.0f / (adsr.decay + 0.001f);
            currentEnvelopeVal += (target - currentEnvelopeVal) * rate * dt;
            if (fabs(currentEnvelopeVal - target) < 0.01f) {
                currentEnvelopeVal = target;
                state = ADSR_SUSTAIN;
            }
            break;
        case ADSR_SUSTAIN:
            // Во время удержания просто плавно стремимся к sustain
            currentEnvelopeVal += (adsr.sustain - currentEnvelopeVal) * 10.0f * dt;
            break;
        case ADSR_RELEASE:
            // Умножаем текущее значение на коэффициент. Оно будет плавно стремиться к нулю.
            rate = 8.0f / (adsr.release + 0.005f);
            currentEnvelopeVal -= currentEnvelopeVal * rate * dt;

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
                // 1Скорость просто равна питчу (1.0 = нормальная скорость)
                // 44100.0f нужно, так как dt у нас в секундах, а курсор в сэмплах
                // Если pitch = 1.0, мы двигаемся на 44100 сэмплов в секунду
                step = pitch *(SPU_SAMPLE_RATE * dt);
                
                // FM модуляция для сэмплов
                float fmFactor = 1.0f + (modInput * 0.1f);
                step *= fmFactor;

                // 2. Движение курсора
                sampleCursor += step;

                // 3. Обработка конца файла
                if (sampleCursor >= len) 
                {
                    if (instrument->loop) 
                    {
                        sampleCursor = fmod(sampleCursor, (float)len);
                    }
                    else 
                    {
                        if (state != ADSR_IDLE && state != ADSR_RELEASE) {
                            state = ADSR_RELEASE;
                            adsr.release = 0.005f; // Форсируем очень быстрое затухание (5 мс)
                        }

                        // Удерживаем курсор на последнем сэмпле, чтобы интерполяции 
                        // было откуда безопасно читать данные, пока идет Release
                        sampleCursor = (float)(len - 1);
                    }
                }
            }
            else {
                // --- ЛОГИКА ДЛЯ СИНТЕЗАТОРА (WAVETABLE) ---

                float fmFactor = 1.0f + (modInput * 0.5f);
                float targetFreq = 261.63f * pitch * fmFactor;
                step = ((float)len * targetFreq) * dt; // dt уже есть (1/SampleRate)

                sampleCursor += step;

                // Синтезатор всегда зациклен (волна)
                if (sampleCursor >= len) sampleCursor -= len;
                if (sampleCursor < 0) sampleCursor += len;
            }


            int i1 = (int)sampleCursor;
            int i2 = i1 + 1;
            int i0 = i1 - 1;
            int i3 = i1 + 2;


            if (isSample) {
                // Для сэмплов не крутим индексы по кругу (кроме случая loop, но он выше обработан)
                // Просто клампим края, чтобы не вылететь за массив
                // i0 (предыдущий) всегда клампим или враппим если надо (но для кубики кламп безопаснее)
                if (i0 < 0) {
                    // Если зациклено - берем с конца, иначе 0
                    i0 = instrument->loop ? (len - 1) : 0;
                }

                if (i1 >= len) 
                    i1 = len - 1;

                if (instrument->loop) 
                {
                    if (i0 < 0) i0 = len - 1;
                    i1 %= len;
                    i2 %= len;
                    i3 %= len;
                }
                else 
                {
                    if (i0 < 0) i0 = 0;
                    if (i1 >= len) i1 = len - 1;
                    if (i2 >= len) i2 = len - 1;
                    if (i3 >= len) i3 = len - 1;
                }
            }
            else 
            {
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
                // WAV (Float -1..1) 
                y0 = instrument->data[i0];
                y1 = instrument->data[i1];
                y2 = instrument->data[i2];
                y3 = instrument->data[i3];
            }
            else {
                // Synth (Short -32k..32k) 
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




class PsxSpu 
{
public:
    unsigned long RateTable[160];
    
    inline static PsxSpu* instance = nullptr;
    
    int sfxCursor = 0;

    SpuVoice voices[SPU_VOICES_COUNT];
    AudioStream stream;
    float* reverbBuffer;
    int reverbCursor = 0, reverbSize = 0;
   // float masterVolume = 0.75f;
    float reverbMix = 0.25f;
    float reverbDecay = 0.4f; // PS1 reverb was quite long
    int currentBufferSize = 1024;//1024;
    int currentVoiceIndex = 0;

    bool voiceFmFlags[SPU_VOICES_COUNT] = { false };


    PsxSpu() {
    }

    ~PsxSpu()
    {
        if (IsAudioStreamValid(stream)) {
            SetAudioStreamCallback(stream, nullptr);
            UnloadAudioStream(stream);
        }
        if (reverbBuffer) {
            free(reverbBuffer);
            reverbBuffer = nullptr;
        }

        instance = nullptr;
    }

    static void AudioCallbackWrapper(void* bufferData, unsigned int frames) {
        if (instance) instance->GenerateAudio((short*)bufferData, frames);
    }


    void initADSR()
    {
        // build the rate table according to Neill's rules
        memset(RateTable, 0, sizeof(unsigned long) * 160);

        uint32_t r = 3;
        uint32_t rs = 1;
        uint32_t rd = 0;

        // we start at pos 32 with the real values... everything before is 0
        for (int i = 32; i < 160; i++) {
            if (r < 0x3FFFFFFF) {
                r += rs;
                rd++;
                if (rd == 5) {
                    rd = 1;
                    rs *= 2;
                }
            }
            if (r > 0x3FFFFFFF)
                r = 0x3FFFFFFF;

            RateTable[i] = r;
        }
    }

    void InitAudioSystem(int bufferSize) 
    {
        if (IsAudioStreamValid(stream)) {
            SetAudioStreamCallback(stream, nullptr); 
            UnloadAudioStream(stream);
        }

        currentBufferSize = bufferSize;
        SetAudioStreamBufferSizeDefault(currentBufferSize);
        stream = LoadAudioStream(SPU_SAMPLE_RATE, 16, 2);

        reverbSize = (int)(SPU_SAMPLE_RATE * 0.35f);
        if (reverbBuffer != nullptr) free(reverbBuffer);
        reverbBuffer = (float*)calloc(reverbSize * 2, sizeof(float));
        reverbCursor = 0;

        instance = this;
        SetAudioStreamCallback(stream, AudioCallbackWrapper);

        PlayAudioStream(stream);

        initADSR();
    }

    void StopAllVoices()
    {
        for (int i = 0; i < SPU_VOICES_COUNT; i++) {
            voices[i].active = false;
            voices[i].state = ADSR_IDLE;
            voices[i].instrument = nullptr; 
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
        if (id == -1) 
        {
            for (int i = 0; i < SPU_VOICES_COUNT; i++) 
            {
                if (voices[i].state == ADSR_RELEASE)
                {
                    id = i;
                    break;
                }
            }
            if (id == -1) {
                id = sfxCursor;
                sfxCursor = (sfxCursor + 1) % SPU_VOICES_COUNT;
                 printf("all voices are busy!\n"); 
            }
        }
        voices[id].active = false; 
        voices[id].NoteOn(inst, pitch, volume, pan, note, progID);
        voiceFmFlags[id] = fmEnabled;

        return id;
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
 
            float bendRange = 2.0f;
            voices[vIdx].pitch = voices[vIdx].basePitch * powf(2.0f, (bend * bendRange) / 12.0f);
        }
    }

    void GenerateAudio(short* buffer, unsigned int frames) 
    {
        if (frames > MAX_AUDIO_BUFFER) frames = MAX_AUDIO_BUFFER;
        float dt = 1.0f / SPU_SAMPLE_RATE;
        for (int i = 0; i < frames; i++)
        {
            float mL = 0, mR = 0;

            for (int v = 0; v < SPU_VOICES_COUNT; v++)
            {
                if (voices[v].active)
                {
                    float l, r;

                    float modIn = 0.0f;
                    int prevV = (v - 1);
                    if (prevV < 0) prevV = SPU_VOICES_COUNT - 1;

                    // Если FM включен для этого голоса, берем сэмпл соседа
                    if (voiceFmFlags[v] && voices[prevV].active)
                    {
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

            fL = tanhf(fL * 0.6f) / 0.6f;
            fR = tanhf(fR * 0.6f) / 0.6f;

            if (fL > 1.0f) fL = 1.0f; else if (fL < -1.0f) fL = -1.0f;
            if (fR > 1.0f) fR = 1.0f; else if (fR < -1.0f) fR = -1.0f;


            buffer[i * 2] = (short)(fL * 32000);
            buffer[i * 2 + 1] = (short)(fR * 32000);
        }

    }

    inline int roundToZero(int val) {
        if (val < 0)
            val = 0;
        return val;
    }
    

    double linearAmpDecayTimeToLinDBDecayTime(double secondsToFullAtten, double targetDb_LeastSquares = 70, double targetDb_InitialSlope = 140)
    {
        if (secondsToFullAtten <= 0.0) return 0.0;

        const double ln10 = 2.302585092994046;
        const double k_short = targetDb_InitialSlope / (20.0 / ln10);
        const double k_long = targetDb_LeastSquares * ln10 / 45.0;

        // Knee near temporal integration (100–150 ms). p controls sharpness.
        const double T_knee = 0.12; // seconds
        const double p = 2.0;

        const double x = secondsToFullAtten / T_knee;
        const double w = 1.0 / (1.0 + std::pow(x, p)); // w≈1 for very short; →0 for long

        return secondsToFullAtten * (w * k_short + (1.0 - w) * k_long);
    }
    
    AdsrSettings MakeADSR(uint16_t ADSR1, uint16_t ADSR2)
    {
        uint8_t Am = (ADSR1 & 0x8000) >> 15;    // if 1, then Exponential, else linear
        uint8_t Ar = (ADSR1 & 0x7F00) >> 8;
        uint8_t Dr = (ADSR1 & 0x00F0) >> 4;
        uint8_t Sl = ADSR1 & 0x000F;
        uint8_t Rm = (ADSR2 & 0x0020) >> 5;
        uint8_t Rr = ADSR2 & 0x001F;

        // The following are unimplemented in conversion (because DLS and SF2 do not support Sustain Rate)
        uint8_t Sm = (ADSR2 & 0x8000) >> 15;
        uint8_t Sd = (ADSR2 & 0x4000) >> 14;
        uint8_t Sr = (ADSR2 >> 6) & 0x7F;


        if (((Am & ~0x01) != 0) ||
            ((Ar & ~0x7F) != 0) ||
            ((Dr & ~0x0F) != 0) ||
            ((Sl & ~0x0F) != 0) ||
            ((Rm & ~0x01) != 0) ||
            ((Rr & ~0x1F) != 0) ||
            ((Sm & ~0x01) != 0) ||
            ((Sd & ~0x01) != 0) ||
            ((Sr & ~0x7F) != 0)) {
            TraceLog(LOG_ERROR,"PSX ADSR parameter(s) out of range"
                "({:#x}, {:#x}, {:#x}, {:#x}, {:#x}, {:#x}, {:#x}, {:#x}, {:#x})",
                Am, Ar, Dr, Sl, Rm, Rr, Sm, Sd, Sr);
            return AdsrSettings();
        }

        // int rateIncTable[8] = {0, 4, 6, 8, 9, 10, 11, 12};
        double samples{ 0 };
        int l;


        // to get the dls 32 bit time cents, take log base 2 of number of seconds * 1200 * 65536
        // (dls1v11a.pdf p25).

      //	if (RateTable[(Ar^0x7F)-0x10 + 32] == 0)
      //		realADSR->attack_time = 0;
      //	else
      //	{
        if ((Ar ^ 0x7F) < 0x10)
            Ar = 0;
        // if linear Ar Mode
        if (Am == 0) {
            uint32_t rate = RateTable[roundToZero((Ar ^ 0x7F) - 0x10) + 32];
            samples = ceil(0x7FFFFFFF / static_cast<double>(rate));
        }
        else if (Am == 1) {
            uint32_t rate = RateTable[roundToZero((Ar ^ 0x7F) - 0x10) + 32];
            samples = 0x60000000 / rate;
            uint32_t remainder = 0x60000000 % rate;
            rate = RateTable[roundToZero((Ar ^ 0x7F) - 0x18) + 32];
            samples += ceil(fmax(0, 0x1FFFFFFF - remainder) / static_cast<double>(rate));
        }


        AdsrSettings adsr;

        adsr.attack = samples / SPU_SAMPLE_RATE;
        //	}

          // Decay Time

        long envelope_level = 0x7FFFFFFF;

        bool bSustainLevFound = false;
        uint32_t realSustainLevel{ 0 };
        // DLS decay rate value is to -96db (silence) not the sustain level
        for (l = 0; envelope_level > 0; l++) {
            if (4 * (Dr ^ 0x1F) < 0x18)
                Dr = 0;
            switch ((envelope_level >> 28) & 0x7) {
            case 0: envelope_level -= RateTable[roundToZero((4 * (Dr ^ 0x1F)) - 0x18 + 0) + 32]; break;
            case 1: envelope_level -= RateTable[roundToZero((4 * (Dr ^ 0x1F)) - 0x18 + 4) + 32]; break;
            case 2: envelope_level -= RateTable[roundToZero((4 * (Dr ^ 0x1F)) - 0x18 + 6) + 32]; break;
            case 3: envelope_level -= RateTable[roundToZero((4 * (Dr ^ 0x1F)) - 0x18 + 8) + 32]; break;
            case 4: envelope_level -= RateTable[roundToZero((4 * (Dr ^ 0x1F)) - 0x18 + 9) + 32]; break;
            case 5: envelope_level -= RateTable[roundToZero((4 * (Dr ^ 0x1F)) - 0x18 + 10) + 32]; break;
            case 6: envelope_level -= RateTable[roundToZero((4 * (Dr ^ 0x1F)) - 0x18 + 11) + 32]; break;
            case 7: envelope_level -= RateTable[roundToZero((4 * (Dr ^ 0x1F)) - 0x18 + 12) + 32]; break;
            default: break;
            }
            if (!bSustainLevFound && ((envelope_level >> 27) & 0xF) <= Sl) {
                realSustainLevel = envelope_level;
                bSustainLevFound = true;
            }
        }
        samples = l;
        adsr.decay = samples / SPU_SAMPLE_RATE;

        // Sustain Rate

        envelope_level = 0x7FFFFFFF;
        // increasing... we won't even bother
        if (Sd == 0) {
            adsr.sustain = -1;
        }
        else {
            if (Sr == 0x7F)
                adsr.sustain = -1;        // this is actually infinite
            else {
                // linear
                if (Sm == 0) {
                    uint32_t rate = RateTable[roundToZero((Sr ^ 0x7F) - 0x0F) + 32];
                    samples = ceil(0x7FFFFFFF / static_cast<double>(rate));
                }
                else {
                    l = 0;
                    // DLS decay rate value is to -96db (silence) not the sustain level
                    while (envelope_level > 0) {
                        long envelope_level_diff{ 0 };
                        long envelope_level_target{ 0 };

                        switch ((envelope_level >> 28) & 0x7) {
                        case 0: envelope_level_target = 0x00000000; envelope_level_diff = RateTable[roundToZero((Sr ^ 0x7F) - 0x1B + 0) + 32]; break;
                        case 1: envelope_level_target = 0x0fffffff; envelope_level_diff = RateTable[roundToZero((Sr ^ 0x7F) - 0x1B + 4) + 32]; break;
                        case 2: envelope_level_target = 0x1fffffff; envelope_level_diff = RateTable[roundToZero((Sr ^ 0x7F) - 0x1B + 6) + 32]; break;
                        case 3: envelope_level_target = 0x2fffffff; envelope_level_diff = RateTable[roundToZero((Sr ^ 0x7F) - 0x1B + 8) + 32]; break;
                        case 4: envelope_level_target = 0x3fffffff; envelope_level_diff = RateTable[roundToZero((Sr ^ 0x7F) - 0x1B + 9) + 32]; break;
                        case 5: envelope_level_target = 0x4fffffff; envelope_level_diff = RateTable[roundToZero((Sr ^ 0x7F) - 0x1B + 10) + 32]; break;
                        case 6: envelope_level_target = 0x5fffffff; envelope_level_diff = RateTable[roundToZero((Sr ^ 0x7F) - 0x1B + 11) + 32]; break;
                        case 7: envelope_level_target = 0x6fffffff; envelope_level_diff = RateTable[roundToZero((Sr ^ 0x7F) - 0x1B + 12) + 32]; break;
                        default: break;
                        }

                        long steps = (envelope_level - envelope_level_target + (envelope_level_diff - 1)) / envelope_level_diff;
                        envelope_level -= (envelope_level_diff * steps);
                        l += steps;
                    }
                    samples = l;
                }
                double timeInSecs = samples / SPU_SAMPLE_RATE;
                adsr.sustain  = /*Sm ? timeInSecs : */linearAmpDecayTimeToLinDBDecayTime(timeInSecs);
            }
        }

        // Sustain Level
        //realADSR->sustain_level = (double)envelope_level/(double)0x7FFFFFFF;//(long)ceil((double)envelope_level * 0.030517578139210854);	//in DLS, sustain level is measured as a percentage
        if (Sl == 0)
            realSustainLevel = 0x07FFFFFF;
        adsr.sustain = realSustainLevel / static_cast<double>(0x7FFFFFFF);

        // If decay is going unused, and there's a sustain rate with sustain level close to max...
        //  we'll put the sustain_rate in place of the decay rate.
        if ((adsr.decay < 2 || (Dr >= 0x0E && Sl >= 0x0C)) && Sr < 0x7E && Sd == 1) {
            adsr.sustain = 0;
            adsr.decay = adsr.sustain;
            //realADSR->decay_time = 0.5;
        }

        // Release Time

        //sustain_envelope_level = envelope_level;

        // We do this because we measure release time from max volume to 0, not from sustain level to 0
        envelope_level = 0x7FFFFFFF;

        // if linear Rr Mode
        if (Rm == 0) {
            uint32_t rate = RateTable[roundToZero((4 * (Rr ^ 0x1F)) - 0x0C) + 32];

            if (rate != 0)
                samples = ceil(static_cast<double>(envelope_level) / rate);
            else
                samples = 0;
        }
        else if (Rm == 1) {
            if ((Rr ^ 0x1F) * 4 < 0x18)
                Rr = 0;
            for (l = 0; envelope_level > 0; l++) {
                switch ((envelope_level >> 28) & 0x7) {
                case 0: envelope_level -= RateTable[roundToZero((4 * (Rr ^ 0x1F)) - 0x18 + 0) + 32]; break;
                case 1: envelope_level -= RateTable[roundToZero((4 * (Rr ^ 0x1F)) - 0x18 + 4) + 32]; break;
                case 2: envelope_level -= RateTable[roundToZero((4 * (Rr ^ 0x1F)) - 0x18 + 6) + 32]; break;
                case 3: envelope_level -= RateTable[roundToZero((4 * (Rr ^ 0x1F)) - 0x18 + 8) + 32]; break;
                case 4: envelope_level -= RateTable[roundToZero((4 * (Rr ^ 0x1F)) - 0x18 + 9) + 32]; break;
                case 5: envelope_level -= RateTable[roundToZero((4 * (Rr ^ 0x1F)) - 0x18 + 10) + 32]; break;
                case 6: envelope_level -= RateTable[roundToZero((4 * (Rr ^ 0x1F)) - 0x18 + 11) + 32]; break;
                case 7: envelope_level -= RateTable[roundToZero((4 * (Rr ^ 0x1F)) - 0x18 + 12) + 32]; break;
                default: break;
                }
            }
            samples = l;
        }
        double timeInSecs = samples / SPU_SAMPLE_RATE;

        //theRate = timeInSecs / sustain_envelope_level;
        //timeInSecs = 0x7FFFFFFF * theRate;	//the release time value is more like a rate.  It is the time from max value to 0, not from sustain level.
        //if (Rm == 0) // if it's linear
        //	timeInSecs *=  LINEAR_RELEASE_COMPENSATION;

        adsr.release = /*Rm ? timeInSecs : */linearAmpDecayTimeToLinDBDecayTime(timeInSecs);


        return adsr;
    }

};