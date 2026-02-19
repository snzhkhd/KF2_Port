#pragma once
#include "ResourceManager.h"

#include "Entity.h"
#include <vector>


namespace Game {
    // Константы размеров (из твоего реверса)
    constexpr int MAX_ENTITIES = 200;
    constexpr int RESOURCE_BUFFER_SIZE = 389120;

    // Глобальные переменные (инкапсулированные в namespace)
    inline std::array<Entity, MAX_ENTITIES> g_Entities;
    inline std::array<uint8_t, RESOURCE_BUFFER_SIZE> g_ResourceBuffer;

    // Те самые странные байтовые буферы из IDA
    inline uint8_t g_Scratchpad_80180138[24380];

    inline int32_t g_FrameCounter = 0;
    inline int16_t g_LanguageID = 0;
    inline int32_t g_NextState = 0; // То, что было *(_DWORD *)(v4 + 416)

    inline Player g_Player;

    inline bool g_RecalculateOrientationCache = true;

    inline PsxRect ScreenSetting;

    // Функции управления
    void InitEventSystem();

    void InitGraphicsSystem();
    void InitSoundSystem();
    void InitProjectileSystem();
    void InitEntitySystem();
    void InitParticleSystem();

    void LoadGameData();

    void InitPlayerInventory();

    void PrecomputeCardinalOrientations();

    void ApplyVideoSettings(PsxRect settings);

    void SetMasterVolume(float volum);

    void InitDialogueFiles();

    bool ShowLoadOrNewGameMenu();

    int TitleScreen();

    int Player_Spawn();

    void ControllerInput();
    void Entities_UpdateAll();
    void UpdatePlayerSystem();
    void ProcessCurrentMode();

    void Draw3DWorld();

    void UpdateUI();

    void ResetState();
}