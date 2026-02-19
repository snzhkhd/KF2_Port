#include "GameContext.h"
#include <iostream>
#include <iomanip> // для красивого вывода таблицы

int MainGameLoop() 
{
    //using namespace Game; // Позволяет использовать имена без Game::
RESET:

    Game::ResetState();

    Game::InitEventSystem();
    Game::InitGraphicsSystem();
    Game::InitSoundSystem();
    Game::InitProjectileSystem();
    Game::InitEntitySystem();
    Game::InitParticleSystem();
    if (!Game::LoadGameData())
        return RETURN_CODE::ERROR_LOAD_DATA;
    Game::InitPlayerInventory();

    Game::g_RecalculateOrientationCache = 1;
    Game::PrecomputeCardinalOrientations();

    Game::ScreenSetting.h = 320;
    Game::ScreenSetting.w = 480;
    Game::ScreenSetting.x = 0;
    Game::ScreenSetting.y = 0;
    
    Game::ApplyVideoSettings(Game::ScreenSetting);

    Game::SetMasterVolume(0.75f);

    if (Game::ShowLoadOrNewGameMenu() == -1) {
        // Логика новой игры
    }

    Game::InitDialogueFiles();

    //wait for start press
    if (Game::TitleScreen())
    {
        Game::Player_Spawn();
    }
    else
        return RETURN_CODE::PRESS_START;

    

    // 3. Главный игровой цикл (RayLib Style)
    while (!WindowShouldClose() && Game::g_NextState == 0) {

        // --- UPDATE ---
        Game::ControllerInput();      // RayLib: IsKeyDown(...)
        Game::Entities_UpdateAll();   // Наша логика AI
        Game::UpdatePlayerSystem();   // Физика игрока
        Game::ProcessCurrentMode();   // Меню/Инвентарь/Игра

        // --- DRAW ---
        BeginDrawing();
        ClearBackground(BLACK);

        // RenderScene(v5, v6, v7);
        Game::Draw3DWorld();      // Здесь будет RayLib BeginMode3D

        Game::UpdateUI();         // Отрисовка HP/MP поверх 3D
        EndDrawing();

        Game::g_FrameCounter++;
    }

    if (Game::g_NextState == 1)
        goto RESET; 

    return RETURN_CODE::QUIT;
}

//MainGameLoop();



VabPair FindVabForSeq(std::shared_ptr<TFile> tFile, int seqIdx) {
    VabPair pair;
    for (int i = seqIdx - 1; i >= 0; i--) {
        const auto& file = tFile->getFile(i);
        FTYPE type = tFile->getEType(file);

        // Если нашли VH, проверяем его размер. 
        // Если он < 5000 байт, скорее всего это мелкий SFX, ищем дальше.
        if (type == FTYPE::VH && file.size() > 5000 && pair.vh == -1) {
            pair.vh = i;
            // VB обычно идет сразу за VH
            if (i + 1 < tFile->getNumFiles()) pair.vb = i + 1;
            break;
        }
    }
    // Если ничего "крупного" не нашли, берем глобальный банк
    if (pair.vh == -1) { pair.vh = 2; pair.vb = 3; }
    return pair;
}

int main()
{

    Game::LoadGameData();
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "KF Sound Tester");

    // !!! ОЧЕНЬ ВАЖНО ДЛЯ ЗВУКА !!!
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_ERROR, "Failed to init audio device!");
        return -1;
    }

    SetTargetFPS(60);

    // 1. Путь к архиву звуков
    // VAB.T обычно содержит пары: (0=Header, 1=Body), (2=Header, 3=Body) и т.д.
    std::string archivePath = "F:/PSX/CHDTOISO-WINDOWS-main/King's Field/CD/COM/VAB.T"; //Western Shore Sequence ?
    auto tFile = ResourceManager::LoadTFile(archivePath);

    AudioSystem audio;

    int currentSeqIdx = 112;

    VabPair pair;
    pair.vh = 2;
    pair.vb = 3;
    auto LoadTrack = [&](int seqIdx) {
        if (seqIdx < 0 || seqIdx >= tFile->getNumFiles()) return;

        // 1. Останавливаем всё старое
        audio.UnloadAll();

        if (pair.vh != -1 && pair.vb != -1) {
            TraceLog(LOG_INFO, "Loading VAB for SEQ %d: VH=%d, VB=%d", seqIdx, pair.vh, pair.vb);
            audio.LoadVab(tFile->getFile(pair.vh), tFile->getFile(pair.vb));
        }
        else {
            // Если рядом ничего нет, пробуем загрузить глобальный банк (T0 и T1)
            audio.LoadVab(tFile->getFile(0), tFile->getFile(1));
        }

        // 3. Запускаем музыку
        audio.PlayMusic(tFile->getFile(seqIdx));
    };

    if (currentSeqIdx != -1) LoadTrack(currentSeqIdx);

    while (!WindowShouldClose()) 
    {

        if (IsKeyPressed(KEY_UP)) {
            // Ищем предыдущий VH выше по списку
            for (int i = pair.vh - 1; i >= 0; i--) {
                if (tFile->getEType(tFile->getFile(i)) == FTYPE::VH) {
                    pair.vh = i;
                    pair.vb = i + 1;
                    audio.LoadVab(tFile->getFile(pair.vh), tFile->getFile(pair.vb));
                    // Перезапускаем текущую музыку с новыми инструментами
                    audio.PlayMusic(tFile->getFile(currentSeqIdx));
                    break;
                }
            }
        }
        if (IsKeyPressed(KEY_DOWN)) {
            // Ищем предыдущий VH выше по списку
            for (int i = pair.vh +1; i >= 0; i++) {
                if (tFile->getEType(tFile->getFile(i)) == FTYPE::VH) {
                    pair.vh = i;
                    pair.vb = i + 1;
                    audio.LoadVab(tFile->getFile(pair.vh), tFile->getFile(pair.vb));
                    // Перезапускаем текущую музыку с новыми инструментами
                    audio.PlayMusic(tFile->getFile(currentSeqIdx));
                    break;
                }
            }
        }

        // Управление: листаем сиквенсы
        if (IsKeyPressed(KEY_PAGE_DOWN)) {
            // Ищем следующий файл типа SEQ
            for (int i = currentSeqIdx + 1; i < tFile->getNumFiles(); i++) {
                if (tFile->getEType(tFile->getFile(i)) == FTYPE::SEQ) {
                    currentSeqIdx = i;
                    LoadTrack(currentSeqIdx);
                    break;
                }
            }
        }
        if (IsKeyPressed(KEY_PAGE_UP)) {
            // Ищем предыдущий файл типа SEQ
            for (int i = currentSeqIdx - 1; i >= 0; i--) {
                if (tFile->getEType(tFile->getFile(i)) == FTYPE::SEQ) {
                    currentSeqIdx = i;
                    LoadTrack(currentSeqIdx);
                    break;
                }
            }
        }
        




        audio.Update();

        BeginDrawing();
        ClearBackground(GetColor(0x181818FF));

        DrawText("King's Field Music Player", 20, 20, 20, LIGHTGRAY);

        DrawText(TextFormat("Current SEQ: %d | VH <%i> VB <%i>", currentSeqIdx, pair.vh, pair.vb), 20, 60, 30, GREEN);
        DrawText("Use PgUp / PgDn to change tracks", 20, 100, 20, GRAY);

        // Визуализация (какие инструменты сейчас активны)
        DrawText("Active MIDI Programs:", 20, 150, 16, RAYWHITE);
        int y = 0;
        for (int i = 0; i < 128; i++) {
            if (audio.IsProgramReady(i)) { // Добавьте такой метод в AudioSystem
                DrawRectangle(20 + (i % 16) * 40, 180 + (i / 16) * 40, 35, 35, DARKGREEN);
                DrawText(TextFormat("%d", i), 25 + (i % 16) * 40, 190 + (i / 16) * 40, 10, WHITE);
            }
        }

        EndDrawing();
    }
  
    CloseAudioDevice();
    CloseWindow();

    return 0;
}