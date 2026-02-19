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
    std::string archivePath = "F:/PSX/CHDTOISO-WINDOWS-main/King's Field/CD/COM/VAB.T";

    // Индекс пары файлов (Bank Index). 0 означает файлы 0 и 1. 2 означает файлы 2 и 3.
    int currentBankIndex = 0;

    // Индекс звука внутри текущего банка
    int currentSoundIndex = 0;

    int OldSoundIndex = 0;

    SoundBank soundBank;
    bool bankLoaded = false;
    std::string statusMessage = "Press ENTER to load bank";

    std::string FileName = "VAB";

    // Функция перезагрузки банка
    auto ReloadBank = [&](int fileIndexVH) {
        // Проверка на четность (VH всегда четный в VAB.T)
        if (fileIndexVH % 2 != 0) fileIndexVH--;
        if (fileIndexVH < 0) fileIndexVH = 0;

        statusMessage = "Loading...";

        // Получаем файлы через ваш ResourceManager
        // ВАЖНО: убедитесь, что LoadTFile и getFile работают
        try {
            auto tFile = ResourceManager::LoadTFile(archivePath);
            if (!tFile || fileIndexVH + 1 >= tFile->getNumFiles()) 
            {
                statusMessage = "Error: Archive end reached";
                FileName = tFile->getFilename();
                return;
            }

            ByteArray& vhData = tFile->getFile(fileIndexVH);
            ByteArray& vbData = tFile->getFile(fileIndexVH + 1);

            if (soundBank.Load(vhData, vbData)) {
                bankLoaded = true;
                currentBankIndex = fileIndexVH;
                currentSoundIndex = 0;
                statusMessage = TextFormat("Loaded Bank #%d (Files %d & %d)",
                    currentBankIndex / 2, fileIndexVH, fileIndexVH + 1);
            }
            else {
                statusMessage = "Error parsing VAB data";
            }
        }
        catch (...) {
            statusMessage = "Exception loading files";
        }
    };

    // Загружаем первый банк при старте
    ReloadBank(0);

    while (!WindowShouldClose())
    {
        // --- УПРАВЛЕНИЕ ---

        // 1. Переключение банков (PageUp / PageDown) - прыгаем через 2 файла
        if (IsKeyPressed(KEY_PAGE_DOWN))
        {
            soundBank.StopAll();
            ReloadBank(currentBankIndex + 2);
        }
        if (IsKeyPressed(KEY_PAGE_UP)) 
        {
            soundBank.StopAll();
            ReloadBank(currentBankIndex - 2);
        }

        // 2. Переключение звуков (Left / Right)
        if (bankLoaded && soundBank.GetSoundCount() > 0) 
        {
            if (IsKeyPressed(KEY_RIGHT)) 
            {
                OldSoundIndex = currentSoundIndex;
                currentSoundIndex++;
                if (currentSoundIndex >= soundBank.GetSoundCount())
                {
                    currentSoundIndex = 0;
                    OldSoundIndex = currentSoundIndex;
                }
                soundBank.StopAll();
                soundBank.Play(currentSoundIndex); // Авто-проигрывание при смене
            }
            if (IsKeyPressed(KEY_LEFT)) 
            {
                OldSoundIndex = currentSoundIndex;
                currentSoundIndex--;
                if (currentSoundIndex < 0)
                {
                    OldSoundIndex = currentSoundIndex;
                    currentSoundIndex = (int)soundBank.GetSoundCount() - 1;
                }
                soundBank.StopAll();
                soundBank.Play(currentSoundIndex);
            }

            // 3. Проигрывание (Space / Enter)
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) 
            {
                if (soundBank.IsPlayering(currentSoundIndex))
                    soundBank.StopAll();
                else
                    soundBank.Play(currentSoundIndex);
            }
        }

        // --- ОТРИСОВКА ---
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("King's Field VAB Player", 10, 10, 20, DARKGRAY);
        DrawText(statusMessage.c_str(), 10, 40, 20, bankLoaded ? DARKGREEN : RED);

        if (bankLoaded) {
            DrawText(TextFormat("Current Bank (Files): %d & %d | %s", currentBankIndex, currentBankIndex + 1, FileName.c_str()), 10, 80, 20, BLACK);
            DrawText(TextFormat("Total Sounds: %zu", soundBank.GetSoundCount()), 10, 110, 20, BLACK);

            DrawRectangle(10, 150, 780, 100, LIGHTGRAY);
            DrawText(TextFormat("SOUND: %d", currentSoundIndex), 30, 170, 40, MAROON);

            DrawText("Controls:", 10, 300, 20, DARKGRAY);
            DrawText("- Left / Right : Select Sound (+Auto Play)", 30, 330, 18, GRAY);
            DrawText("- Space : Play Current Sound Again", 30, 350, 18, GRAY);
            DrawText("- PageUp / PageDn : Next/Prev Bank (File Pair)", 30, 370, 18, GRAY);
        }
        else {
            DrawText("No bank loaded or file error.", 10, 100, 20, RED);
        }

        EndDrawing();
    }

    // Очистка
 //   soundBank.UnloadAll();
    CloseAudioDevice();
    CloseWindow();

    return 0;
}