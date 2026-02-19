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
    Game::LoadGameData();
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
        return 1;

    

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

    return 0;
}

//MainGameLoop();


void UpdateCurrentTexture(const std::shared_ptr<TextureDB>& db,
    size_t texIndex,
    Texture2D& outTex)
{
    if (!db) return; // Защита от null
    if (texIndex >= db->getTextureCount()) return; // Защита индекса

    if (outTex.id != 0) UnloadTexture(outTex);

    const auto& kfTex = db->getTexture(texIndex);
    if (kfTex.image.data != nullptr) {
        outTex = LoadTextureFromImage(kfTex.image);
        TraceLog(LOG_INFO, "Switched to sub-texture %zu / %zu",
            texIndex + 1, db->getTextureCount());
    }
}

// Перезагрузить TextureDB для нового файла в архиве
void ReloadTextureDB(const std::string& archivePath,
    int targetFileIndex,      // Какой файл из .T архива грузим
    size_t& outTexIndex,      // Ссылка на текущий индекс текстуры (сбрасываем в 0)
    std::shared_ptr<TextureDB>& outDB,
    Texture2D& outRaylibTex)
{
    // 1. Очищаем старую GPU текстуру
    if (outRaylibTex.id != 0) {
        UnloadTexture(outRaylibTex);
        outRaylibTex = { 0 };
    }

    // 2. Загружаем новый DB (используем targetFileIndex!)
    outDB = ResourceManager::LoadKFTextures(archivePath, targetFileIndex);

    // 3. Сбрасываем индекс под-текстуры на 0, так как открыли новый файл
    outTexIndex = 0;

    // 4. Если загрузка удалась и там есть текстуры
    if (outDB && outDB->getTextureCount() > 0) {
        // Берем ПЕРВУЮ (0) текстуру из нового файла
        const auto& kfTex = outDB->getTexture(0);

        if (kfTex.image.data != nullptr) {
            outRaylibTex = LoadTextureFromImage(kfTex.image);
        }

        TraceLog(LOG_INFO, "Reloaded File #%d: contains %zu textures",
            targetFileIndex, outDB->getTextureCount());
    }
    else {
        // Если вернулся nullptr, значит это звук, модель или конец файла
        TraceLog(LOG_WARNING, "File #%d is NOT a texture or is empty", targetFileIndex);
    }
}

// Отрисовка кадра (вынесено для чистоты main)
void DrawFrame(const std::shared_ptr<TextureDB>& db, size_t texIndex,
    const Texture2D& raylibTex, const std::string& archivePath,
    int fileIndex, int windowWidth, int windowHeight)
{
    BeginDrawing();
    ClearBackground(GetColor(0x181818FF));

    bool isValidTexture = (db != nullptr) && (db->getTextureCount() > 0) && (raylibTex.id != 0);

    if (isValidTexture) 
    {
        const auto& kfTex = db->getTexture(texIndex);

        // Центрирование и масштабирование
        float texX = (windowWidth - raylibTex.width) / 2.0f;
        float texY = (windowHeight - raylibTex.height) / 2.0f - 40;

        float scale = 1.0f;
        if (raylibTex.width > windowWidth - 40 ||
            raylibTex.height > windowHeight - 120) {
            float scaleX = (windowWidth - 40) / static_cast<float>(raylibTex.width);
            float scaleY = (windowHeight - 120) / static_cast<float>(raylibTex.height);
            scale = fminf(scaleX, scaleY);
        }

        DrawTextureEx(raylibTex, Vector2{ texX, texY }, 0.0f, scale, WHITE);

        // Инфо-панель
        DrawRectangle(0, windowHeight - 90, windowWidth, 90, GetColor(0x282828DD));

        DrawText(TextFormat("File: %d | Texture: %zu / %zu",
            fileIndex, texIndex + 1, db->getTextureCount()),
            20, windowHeight - 80, 20, WHITE);

        DrawText(TextFormat("Size: %dx%d | VRAM: (%d,%d)",
            kfTex.pxWidth, kfTex.pxHeight,
            kfTex.pxVramX, kfTex.pxVramY),
            20, windowHeight - 55, 16, LIGHTGRAY);

        const char* pModeStr = "Unknown";
        switch (kfTex.pMode) {
        case PixelMode::CLUT4Bit:  pModeStr = "CLUT4Bit"; break;
        case PixelMode::CLUT8Bit:  pModeStr = "CLUT8Bit"; break;
        case PixelMode::Direct15Bit: pModeStr = "Direct15Bit"; break;
        case PixelMode::Direct24Bit: pModeStr = "Direct24Bit"; break;
        case PixelMode::Mixed: pModeStr = "Mixed"; break;
        }
        DrawText(TextFormat("Mode: %s | CLUT: %s",
            pModeStr, kfTex.hasClut ? "Yes" : "No"),
            20, windowHeight - 35, 16, LIGHTGRAY);

        // Превью палитры
        if (kfTex.hasClut && !kfTex.clutColorTable.empty()) {
            DrawText("CLUT:", windowWidth - 180, windowHeight - 80, 14, LIGHTGRAY);
            int swatchSize = 12;
            for (int i = 0; i < std::min(static_cast<int>(kfTex.clutColorTable.size()), 16); ++i) {
                Color c = kfTex.clutColorTable[i];
                DrawRectangle(windowWidth - 170 + (i % 8) * 14,
                    windowHeight - 62 + (i / 8) * 14,
                    swatchSize, swatchSize, c);
                DrawRectangleLines(windowWidth - 170 + (i % 8) * 14,
                    windowHeight - 62 + (i / 8) * 14,
                    swatchSize, swatchSize, DARKGRAY);
            }
        }
    }
    else {
        // ОТРИСОВКА, ЕСЛИ ФАЙЛ НЕ ТЕКСТУРА
        DrawText(TextFormat("File Index: %d", fileIndex), 20, windowHeight - 80, 20, WHITE);

        const char* msg = (db == nullptr) ? "NOT A TEXTURE (Audio/Model)" : "EMPTY TEXTURE DATA";
        DrawText(msg, windowWidth / 2 - MeasureText(msg, 20) / 2, windowHeight / 2, 20, RED);
    }

    // Подсказки
    DrawText("←→/A/D/Wheel: Tex | PgUp/PgDn: File | R:Reload | ESC:Exit",
        20, 10, 16, GRAY);
    DrawFPS(windowWidth - 60, 10);

    EndDrawing();
}


int main()
{
    // === Инициализация RayLib ===
    const int windowWidth = 1024;
    const int windowHeight = 768;

    InitWindow(1024, 768, "KF Texture Viewer");
    SetTargetFPS(60);

    std::string archivePath = "F:/PSX/CHDTOISO-WINDOWS-main/King's Field/CD/COM/RTIM.T";
    int currentFileIndex = 0;
    size_t currentTexIndex = 0; // Индекс внутри файла (обычно 0)

    std::shared_ptr<TextureDB> textureDB;
    Texture2D currentRaylibTex = { 0 };

    // Первичная загрузка
    ReloadTextureDB(archivePath, currentFileIndex, currentTexIndex, textureDB, currentRaylibTex);

    while (!WindowShouldClose()) {

        // 1. Навигация внутри файла (если в файле > 1 картинки)
        if (textureDB && textureDB->getTextureCount() > 1) {
            bool changed = false;
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                currentTexIndex = (currentTexIndex + 1) % textureDB->getTextureCount();
                changed = true;
            }
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                currentTexIndex = (currentTexIndex == 0) ? textureDB->getTextureCount() - 1 : currentTexIndex - 1;
                changed = true;
            }
            if (changed) {
                UpdateCurrentTexture(textureDB, currentTexIndex, currentRaylibTex);
            }
        }

        // 2. Навигация по файлам архива (0..378)
        bool fileChanged = false;
        if (IsKeyPressed(KEY_PAGE_DOWN)) {
            currentFileIndex++;
            fileChanged = true;
        }
        if (IsKeyPressed(KEY_PAGE_UP)) {
            if (currentFileIndex > 0) currentFileIndex--;
            fileChanged = true;
        }
        if (IsKeyPressed(KEY_R)) {
            fileChanged = true;
        }

        if (fileChanged) {
            // ВАЖНО: передаем currentTexIndex по ссылке, функция сбросит его в 0
            ReloadTextureDB(archivePath, currentFileIndex, currentTexIndex, textureDB, currentRaylibTex);
        }

        DrawFrame(textureDB, currentTexIndex, currentRaylibTex, archivePath, currentFileIndex, GetScreenWidth(), GetScreenHeight());
    }

    if (currentRaylibTex.id != 0) UnloadTexture(currentRaylibTex);
    CloseWindow();
    return 0;
}