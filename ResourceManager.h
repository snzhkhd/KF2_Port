#pragma once
#include "TextureDB.h"
#include <memory>
#include <unordered_map>

#include "tfile.h"

struct AnimationData {
    ModelAnimation* anims = nullptr;
    int count = 0;
};

class ResourceManager {
public:
    // Поиск оригинальных ресурсов по индексу (как в функции LoadFileByIndex)
    static std::shared_ptr<Model> GetModelByIndex(int index);
    static std::shared_ptr<Texture2D> GetTextureByVram(int x, int y);

    // Специальный загрузчик для таблиц данных (Items, Spells)
    static void LoadGameDatabases(const std::string& path);

    //KF
    static std::shared_ptr<TFile> LoadTFile(const std::string& path);
    static std::shared_ptr<TextureDB> LoadKFTextures(const std::string& path, int index = 0);

    // Удобный метод для получения конкретного файла из архива
    static ByteArray& GetFileFromT(const std::string& archivePath, size_t fileIndex);



    static std::shared_ptr<Texture2D> LoadTexture(const std::string& path, bool GetMipMap = false, TextureFilter filter = TEXTURE_FILTER_POINT, TextureWrap wrap = TEXTURE_WRAP_REPEAT);
    static std::shared_ptr<Model> LoadModel(const std::string& path);
    static std::shared_ptr<AnimationData> LoadModelAnimation(const std::string& path);

    // --- Audio ---
    static std::shared_ptr<Sound> LoadSound(const std::string& path);
    static std::shared_ptr<Wave> LoadWavs(const std::string& path);
    static std::shared_ptr<Music> LoadMusic(const std::string& path);

    // --- Fonts ---
    static std::shared_ptr<Font> LoadFont(const std::string& path, int fontSize = 32);

    static void UnloadAll();
private:
    ResourceManager() = delete; // Чисто статический класс

    static std::unordered_map<std::string, std::shared_ptr<TFile>> tfiles_;
    static std::unordered_map<std::string, std::shared_ptr<TextureDB>> kftexture_;


    static std::unordered_map<std::string, std::shared_ptr<Texture2D>> textures_;

    static std::unordered_map<std::string, std::shared_ptr<Model>> models_;
    static std::unordered_map<std::string, std::shared_ptr<AnimationData>> animations_;
    static std::unordered_map<std::string, std::shared_ptr<Sound>> sounds_;
    static std::unordered_map<std::string, std::shared_ptr<Wave>> waves_;
    static std::unordered_map<std::string, std::shared_ptr<Music>> musics_;
    static std::unordered_map<std::string, std::shared_ptr<Font>> fonts_;
};


