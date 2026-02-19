#include "GameContext.h"

void Game::ResetState()
{
    ResourceManager::UnloadAll();
    //std::memset(&g_Entities, 0, sizeof(g_Entities));
    //std::memset(&EquipmentDatabase, 0, sizeof(FileDescriptors));
    //std::memset(&MagicDefinitionTable, 0, sizeof(MagicDefinitionTable));
    //std::memset(&ScreenSetting, 0, sizeof(ScreenSetting));
    //std::memset(&ResourceBuffer, 0, sizeof(ResourceBuffer));
    //std::memset(&SpellEntryTable, 0, sizeof(SpellEntryTable));
    //std::memset(&CurrentEntityDef, 0, sizeof(CurrentEntityDef));
}


void Game::InitEventSystem()
{
}

void Game::InitGraphicsSystem()
{
}

void Game::InitSoundSystem()
{
}

void Game::InitProjectileSystem()
{
}

void Game::InitEntitySystem()
{
}

void Game::InitParticleSystem()
{
}

void Game::LoadGameData()
{
    ResourceManager::LoadGameDatabases("./CD/COM/FDAT.T");
}

void Game::InitPlayerInventory()
{
}

void Game::PrecomputeCardinalOrientations()
{
}

void Game::ApplyVideoSettings(PsxRect settings)
{
}

void Game::SetMasterVolume(float volum)
{
}

void Game::InitDialogueFiles()
{
}

bool Game::ShowLoadOrNewGameMenu()
{
    return false;
}

int Game::TitleScreen()
{

    BeginDrawing();
    ClearBackground(Color(127,127,127));    //99, 99, 99


    EndDrawing();
    return 1;
}

int Game::Player_Spawn()
{
    return 1;
}

void Game::ControllerInput()
{
}

void Game::Entities_UpdateAll()
{
}

void Game::UpdatePlayerSystem()
{
}

void Game::ProcessCurrentMode()
{
}

void Game::Draw3DWorld()
{
}

void Game::UpdateUI()
{
}
