#include "ResourceManager.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include "math.h"
#include "utilities.h"
static const double K0[] = { 0.0, 0.9375, 1.796875, 1.53125, 1.90625 };
static const double K1[] = { 0.0, 0.0, -0.8125, -0.859375, -0.9375 };


AudioSystem::AudioSystem()
{
    music[0] = { 112, {2,3} };
    music[1] = { 113, {4,5} };
    music[2] = { 114, {6,7} };
    music[3] = { 115, {8,9} };
    music[4] = { 116, {10,11} };
    music[5] = { 117, {12,13} };
    music[6] = { 118, {14,15} };
    music[7] = { 119, {16,17} };
    music[8] = { 120, {18,19} };
    music[9] = { 121, {20,21} };
}

AudioSystem::~AudioSystem()
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

void AudioSystem::InitSPUSystem()
{
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_ERROR, "Failed to init audio device!");
        return;
    }
    spu.InitAudioSystem(1024);
}

void AudioSystem::PlaySEQMusic(int id)
{
    CurrentSeqId = id;
    if (id < 0)
        id = 8;
    else if (id > 8)
        id = 0;
    std::string archivePath = "F:/PSX/CHDTOISO-WINDOWS-main/King's Field/CD/COM/VAB.T";

    auto tFile = ResourceManager::LoadTFile(archivePath);

    LoadVab(tFile->getFile(music[id].pair.vh), tFile->getFile(music[id].pair.vb));

    // 3. Запускаем музыку
    PlayMusic(tFile->getFile(music[id].SeqId));
}


bool AudioSystem::Load(const ByteArray& vhData, const ByteArray& vbData)
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
                if (IsSoundReady(sound))
                {
                        sounds.push_back(sound);
                }
            }
        }
        currentOffset += vagSize;
    }

    return !sounds.empty();
}

void AudioSystem::UnloadAll()
{
    // 1. Останавливаем дирижера
    seqPlayer.StopAll();

    // 2. Выключаем голоса в SPU
    spu.StopAllVoices();

    // 3. Очищаем данные сэмплов внутри каждой программы
    for (int i = 0; i < 128; i++) {
        for (int t = 0; t < 16; t++) {
            // Очищаем вектор float, это реально освобождает память
            programs[i].tones[t].data.clear();
            programs[i].tones[t].sampleCount = 0;
        }
        programs[i].toneCount = 0;
    }

    // 4. Очищаем вектор sounds, если вы в него что-то пушили для SFX
    sounds.clear();
}

void AudioSystem::PlaySfx(int index, float volume, float pitch)
{
    if (index >= 0 && index < sounds.size())
    {
        if (IsSoundValid(sounds[index]))
        {
            SetSoundPitch(sounds[index], pitch);
            SetSoundVolume(sounds[index], volume);
            PlaySound(sounds[index]);
        }


    }
}

int AudioSystem::PlayMusic(const ByteArray& seqData)
{
    
    return seqPlayer.Play(seqData, 0);
}

void AudioSystem::Update()
{
    seqPlayer.Update(GetFrameTime(), this);
}

void AudioSystem::PlaySample(int program, float note, float volume, float pan, int channel)
{
    if (program < 0 || program >= 128) return;
    Program& prog = programs[program];

    // Играем ВСЕ тона, которые подходят под эту ноту
    for (int i = 0; i < prog.toneCount; ++i) {
        const Tone& tone = prog.tones[i];
        if (note >= tone.minNote && note <= tone.maxNote) {
            float shift = (note - (float)tone.centerNote) + ((float)tone.fineTune / 128.0f);
            float pitch = powf(2.0f, shift / 12.0f);

            // Используем громкость конкретного тона
            float finalVol = volume * ((float)tone.vol / 127.0f) * 0.7f;

            spu.PlayNote(&tone, pitch, finalVol, false, pan, (int)note, program);
        }
    }
}


bool AudioSystem::IsProgramReady(int program)
{
    if (program < 0 || program >= 128) return false;
    return programs[program].toneCount > 0;
}


bool AudioSystem::LoadVab(const ByteArray& vhData, const ByteArray& vbData)
{
    UnloadAll();
    for (int i = 0; i < 128; i++) programs[i].toneCount = 0;

    if (vhData.size() < 2080) return false;

    // 1. Заголовки
    uint16_t progCount = *reinterpret_cast<const uint16_t*>(vhData.data() + 18);
    uint16_t vagCount = *reinterpret_cast<const uint16_t*>(vhData.data() + 22);

    // --- ЭТАП 1: НАРЕЗКА (Делим на 32768) ---
    std::vector<std::vector<float>> allSamples;
    size_t offsetTableAddr = 2080 + (static_cast<size_t>(progCount) * 512) + 2;
    const uint16_t* sizeTable = reinterpret_cast<const uint16_t*>(vhData.data() + offsetTableAddr);
    uint32_t currentOffset = 0;

    for (int i = 0; i < vagCount; ++i) {
        uint32_t vagSize = static_cast<uint32_t>(sizeTable[i]) * 8;
        std::vector<float> floatData;
        if (vagSize > 16) {
            std::vector<int16_t> pcm = DecodeADPCM(vbData.data() + currentOffset, vagSize);
            for (int16_t s : pcm) floatData.push_back((float)s / 32768.0f);
        }
        allSamples.push_back(floatData);
        currentOffset += vagSize;
    }

    uint8_t vabMainVol = vhData[24];
    this->currentVabMasterVol = (float)vabMainVol / 127.0f;

    

    // --- ЭТАП 2: МАППИНГ (Sony libsnd Standard) ---
    const uint8_t* progAttrPtr = vhData.data() + 32;
    const uint8_t* toneAttrPtr = vhData.data() + 2080;

    int totalMapped = 0;
    for (int p = 0; p < 128; p++) {
        uint8_t numTones = progAttrPtr[p * 16];
        if (numTones == 0) continue;

        uint8_t progVol = progAttrPtr[p * 16 + 1];

        const uint8_t* toneGroup = toneAttrPtr + (p * 512);
        int added = 0;
        for (int t = 0; t < 16; t++) 
        { 
            const uint8_t* toneData = toneGroup + (t * 32);
            uint16_t vagID = *reinterpret_cast<const uint16_t*>(toneData + 22);

            if (vagID -1 < allSamples.size() && !allSamples[vagID].empty())
            {
                Tone& tone = programs[p].tones[added];
                tone.data = allSamples[vagID -1];
                
                tone.sampleCount = (uint32_t)tone.data.size();
                tone.type = InstrumentType::Sample;
                tone.loop = false;

                uint8_t toneVol = toneData[2];
                float volFactor = ((float)progVol / 127.0f) * ((float)toneVol / 127.0f);
                tone.vol = (uint8_t)(volFactor * 127.0f);
                if (tone.vol == 0)
                {
                    TraceLog(LOG_WARNING, "tone vol = 0!  set 100");
                    tone.vol = 100; // Защита
                }

                // Важно: в KF min/max могут быть перепутаны
                uint8_t n1 = toneData[6];
                uint8_t n2 = toneData[7];
                tone.minNote = (n1 < n2) ? n1 : n2;
                tone.maxNote = (n1 > n2) ? n1 : n2;
                // Если в KF min > max (например 127 и 0), меняем их местами
                if (tone.minNote > tone.maxNote) std::swap(tone.minNote, tone.maxNote);

                // Если 0 и 0 (или 0 и 127), ставим полный диапазон
                if (tone.maxNote == 0) tone.maxNote = 127;

                tone.centerNote = toneData[4];
                if (tone.centerNote == 0) tone.centerNote = 60;
                tone.fineTune = (int8_t)toneData[5];


                uint16_t ADSR1 = *reinterpret_cast<const uint16_t*>(toneData + 16);
                uint16_t ADSR2 = *reinterpret_cast<const uint16_t*>(toneData + 18);

                AdsrSettings asdr = spu.MakeADSR(ADSR1, ADSR2);

                tone.attack = asdr.attack;  // 5мс (быстро)
                tone.decay = asdr.decay;     // Неважно
                tone.sustain = asdr.sustain;   // Полная громкость
                tone.release = asdr.release;   // Средний хвост


               // printf("a <%f> | d <%f> | s <%f> r | <%f>\n", tone.attack, tone.decay, tone.sustain, tone.release);
                added++;
            }
        }
        programs[p].toneCount = added;
        if (added > 0) totalMapped++;
    }
    return totalMapped > 0;
}

void AudioSystem::Play(int index)
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

void AudioSystem::Stop(int index)
{
    if (index >= 0 && index < sounds.size()) 
    {
        if (IsSoundValid(sounds[index]))
            StopSound(sounds[index]);
    }

}

void AudioSystem::StopAll()
{
    for (auto& s : sounds)
    {
        if (IsSoundValid(s)) 
            StopSound(s);
    }

    
}

bool AudioSystem::IsPlayering(int index)
{
    if (index >= 0 && index < sounds.size())
        return IsSoundValid(sounds[index]) && IsSoundPlaying(sounds[index]);
    else
        return false;
}

bool AudioSystem::IsSoundReady(Sound s)
{
    return IsSoundValid(s);
}

Sound AudioSystem::GetSound(int index)
{
    if (index >= 0 && index < sounds.size()) 
    {
        if(IsSoundValid(sounds[index]))
            return sounds[index];
    }
    return { 0 };
}


std::vector<int16_t> AudioSystem::DecodeADPCM(const uint8_t* src, size_t size)
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

void AudioSystem::SetPitchBend(int channel, float bend)
{
    if (channel < 0 || channel >= 16) return;

    channelBends[channel] = bend;

    // Обновляем все голоса, которые сейчас звучат на этом канале
    for (int i = 0; i < SPU_VOICES_COUNT; i++) {
        if (spu.voices[i].active && spu.voices[i].parentProgramID == channel) {
            spu.UpdateVoicePitch(i, bend);
        }
    }
}

void AudioSystem::NoteOff(int prog, int note)
{
    for (int i = 0; i < SPU_VOICES_COUNT; i++) {
        auto& v = spu.voices[i];
        if (v.active && v.parentProgramID == prog && v.currentNote == (uint8_t)note) {
            v.NoteOff(); // Теперь голос уйдет в Release
        }
    }
}

int SeqPlayer::FindFreeSlot()
{
    for (int i = 0; i < 16; ++i)
    {
        if (!slots[i].active)
            return i;
    }
    return -1;
}

bool SeqPlayer::Load(const ByteArray& data, int slot)
{
    if (data.size() < 15) return false;

    // Проверка магического числа (83 = 'S', 112 = 'p' в зависимости от порядка)
    if (Utilities::fileIsSEQ(data)) {
        std::cerr << "This is not SEQ Data." << std::endl;
        return false;
    }
    
    // 2. Проверка версии (в IDA: seqData[7] == 1)
    uint32_t version = data[7];
    if (version != 1) {
        std::cerr << "Unsupported SEQ version: " << version << std::endl;
        return false;
    }

    // 3. Чтение Resolution (TPQN) - смещение 8 и 9
    // В IDA: v10 = seqData[8]; v5+74 = seqData[9] | (v10 << 8)
    // Это Big-Endian!
    int slotIdx = FindFreeSlot();
    if (slotIdx == -1) return -1;

    SeqSlot& s = slots[slotIdx];
    s.resolution = (static_cast<uint16_t>(data[8]) << 8) | data[9];

    // 4. Чтение темпа (Микросекунды) - смещение 10, 11, 12
    // В IDA: (v12 << 16) | (v13 << 8) | v11[2]
    uint32_t rawTempo = (static_cast<uint32_t>(data[10]) << 16) |
        (static_cast<uint32_t>(data[11]) << 8) |
        data[12];

    // Формула из IDA: v14 = 60,000,000 / rawTempo
    if (rawTempo > 0) {
        s.tempo = rawTempo;
       // s.bpm = 60000000.0 / static_cast<double>(rawTempo);
    }

    // 5. Начало данных
    // В IDA указатель смещается: *(_DWORD *)(v5 + 4) = v11 + 3;
    // Обычно данные SEQ начинаются с 15-го байта (0x0F)
    //s.dataOffset = 15;

    // Инициализация каналов как в IDA (цикл do-while до 16)
    for (int i = 0; i < 16; ++i) {
        s.channels[i].volume = 127; // v6 + 78 = 127
        s.channels[i].pan = 64;    // v7 + 23 = 64
    }

    return true;
}


void SeqPlayer::StopAll() {
    for (int i = 0; i < 16; ++i) {
        slots[i].active = false;
        slots[i].pData = nullptr;
    }
}

SeqPlayer::SeqPlayer()
{
    for (int i = 0; i < 16; ++i) slots[i].active = false;
}


void SeqPlayer::Update(float deltaTime, AudioSystem* system)
{
    for (int i = 0; i < 16; ++i) {
        if (!slots[i].active) continue;
        SeqSlot& s = slots[i];

        float bpm = 60000000.0f / (float)s.tempo;
        float ticksPerSec = (bpm * (float)s.resolution) / 60.0f;
        s.tickAccumulator += deltaTime * ticksPerSec;

        // Обрабатываем ВСЕ события, у которых Delta Time = 0
        while (s.active && s.tickAccumulator >= (float)s.waitTicks) {
            s.tickAccumulator -= (float)s.waitTicks;
            if (!ParseNextEvent(s, system)) {
                s.active = false;
            //    break;
            }
            // ParseNextEvent уже прочитал новый waitTicks в конце.
            // Если он 0, цикл выполнится снова мгновенно.
        }
    }
}

int SeqPlayer::Play(const ByteArray& data, uint16_t vabID)
{
    if (data.size() < 15)
    {
        printf(" SeqPlayer::Play data.size() < 15  \n");
        return -1;
    }

    if (!Utilities::fileIsSEQ(data)) {
        std::cerr << "Not a valid King's Field SEQ file." << std::endl;
        return -1;
    }

    int slotIdx = FindFreeSlot();
    if (slotIdx == -1)
    {
        printf(" SeqPlayer::Play slotIdx == -1  \n");
        return -1;
    }

    SeqSlot& s = slots[slotIdx];

    s.pData = data.data();
    s.dataSize = (uint32_t)data.size();
    s.vabID = vabID;

    // Читаем заголовок
    s.resolution = (static_cast<uint16_t>(data[8]) << 8) | data[9];
    uint32_t rawTempo = (static_cast<uint32_t>(data[10]) << 16) |
        (static_cast<uint32_t>(data[11]) << 8) |
        data[12];
    s.tempo = (rawTempo > 0) ? rawTempo : 500000;

    // Инициализируем каналы
    for (int i = 0; i < 16; ++i) {
        s.channels[i].volume = 127;
        s.channels[i].program = 0;
    }
    // -------------------------------

    s.currentPos = 15;    // Начало данных
    s.runningStatus = 0;  // Сброс статуса
    s.active = true;
    s.tickAccumulator = 0;

    uint8_t seqMasterVol = data[13]; // Обычно здесь мастер-громкость
    s.masterVolFactor = (float)seqMasterVol / 127.0f;
    if (s.masterVolFactor <= 0) s.masterVolFactor = 1.0f;

    
    // Теперь pData не пустой, и ReadVLQ сработает!
    s.waitTicks = ReadVLQ(s.pData, s.currentPos, s.dataSize);
    //printf("masterVolFactor <%f>  \t\r", s.masterVolFactor);
    //TraceLog(LOG_INFO, "SEQ Started: %d bytes, Resolution=%d", s.dataSize, s.resolution);

    return slotIdx;
}

bool SeqPlayer::ParseNextEvent(SeqSlot& s, AudioSystem* system)
{
    if (s.currentPos >= s.dataSize) return false;

    // --- 1. ЧИТАЕМ СТАТУС-БАЙТ ---
    uint8_t status = s.pData[s.currentPos++];

    // Running Status
    if (status < 0x80) {
        status = s.runningStatus;
        s.currentPos--;
    }
    else if (status < 0xF0) {
        s.runningStatus = status;
    }

    uint8_t event = status & 0xF0;
    uint8_t chan = status & 0x0F;

    // --- 2. ВЫПОЛНЯЕМ КОМАНДУ ---
    if (event == 0x90) { // Note On
        uint8_t note = s.pData[s.currentPos++];
        uint8_t velocity = s.pData[s.currentPos++];
        if (velocity > 0) 
        {
            uint8_t rawPan = s.channels[chan].pan;

            // 2. Переводим в float (0.0 - лево, 1.0 - право)
            float pan = (float)rawPan / 127.0f;
            float vol = ((float)velocity / 127.0f) *
                ((float)s.channels[chan].volume / 127.0f) *
                ((float)s.channels[chan].expression / 127.0f)
                    * system->currentVabMasterVol;
            system->PlaySample(s.channels[chan].program, (float)note, vol, pan, chan);
        }
        else
            system->NoteOff(s.channels[chan].program, note);
    }
    else if (event == 0x80)
    { // Note Off
        //s.currentPos += 2;
        uint8_t note = s.pData[s.currentPos++];
        uint8_t vel = s.pData[s.currentPos++];

        // Передаем команду "отпустить клавишу" в AudioSystem
        system->NoteOff(s.channels[chan].program, note);
    }
    else if (event == 0xB0) { // Control Change
        uint8_t controller = s.pData[s.currentPos++];
        uint8_t value = s.pData[s.currentPos++];

        if (controller == 7) { // Volume
            s.channels[chan].volume = value;
        }
        else if (controller == 10) { // Pan (0..127, 64 = Center)
            s.channels[chan].pan = value;
        }
        else if (controller == 11) { // Expression
            s.channels[chan].expression = value; 
        }
    }
    else if (event == 0xC0) { s.channels[chan].program = s.pData[s.currentPos++]; }
    else if (event == 0xE0) { // Pitch Bend
        uint8_t lsb = s.pData[s.currentPos++];
        uint8_t msb = s.pData[s.currentPos++];
        // Сохраняем значение бенда (-8192 до +8191)
        int bendValue = ((msb << 7) | lsb) - 8192;
        // Передаем в систему, чтобы она обновила питч играющих голосов
        
        
        system->SetPitchBend(chan, (float)bendValue / 8192.0f);
    }
    else if (status == 0xFF) {
        uint8_t type = s.pData[s.currentPos++];
        uint8_t len = s.pData[s.currentPos++];
        if (type == 0x2F)
        {
            printf(" SeqPlayer: Track finished. Looping...\n");

            // 1. Возвращаем курсор на начало данных (сразу после заголовка)
            s.currentPos = 15;

            // 2. Сбрасываем накопитель времени и статус
            s.tickAccumulator = 0;
            s.runningStatus = 0;

            // 3. Читаем задержку самого первого события заново
            if (s.currentPos < s.dataSize) {
                s.waitTicks = ReadVLQ(s.pData, s.currentPos, s.dataSize);
            }
            else {
                return false; // Если данных почему-то нет
            }

            // 4. Возвращаем true, чтобы слот НЕ деактивировался
            return true;
        }
        else if (type == 0x51) {
            // TEMPO CHANGE: В PS1 SEQ нет байта длины для темпа!
            // Сразу читаем 3 байта данных
            uint32_t val = (s.pData[s.currentPos] << 16) |
                (s.pData[s.currentPos + 1] << 8) |
                s.pData[s.currentPos + 2];
            s.currentPos += 3;
            s.tempo = val;
        }
        else {
            // Для неизвестных событий пробуем прочитать длину (как в MIDI), 
            // но в PS1 это редкость.
            //uint8_t len = s.pData[s.currentPos++];
            s.currentPos += len;
        }
    }

    // --- 3. ЧИТАЕМ ЗАДЕРЖКУ ДО СЛЕДУЮЩЕГО СОБЫТИЯ ---
    if (s.currentPos < s.dataSize) {
        s.waitTicks = ReadVLQ(s.pData, s.currentPos, s.dataSize);
    }
    else {
        return false;
    }

    return true;
}

uint32_t SeqPlayer::ReadVLQ(const uint8_t* data, uint32_t& pos, uint32_t size)
{
    uint32_t value = 0;
    uint8_t byte;
    int safety = 0;
    do {
     //   if (pos >= size) return 0; // Конец данных
        byte = data[pos++];
        value = (value << 7) | (byte & 0x7F);
        if (++safety > 4) break; // Защита от бесконечного цикла
    } while (byte & 0x80);
    return value;
}
