#include "ResourceManager.h"
#include <iostream>
#include "TextureDB.h"

std::unordered_map<std::string, std::shared_ptr<TextureDB>> ResourceManager::kftexture_;
std::unordered_map<std::string, std::shared_ptr<TFile>> ResourceManager::tfiles_;

std::unordered_map<std::string, std::shared_ptr<Texture2D>> ResourceManager::textures_;
std::unordered_map<std::string, std::shared_ptr<Model>> ResourceManager::models_;
std::unordered_map<std::string, std::shared_ptr<AnimationData>> ResourceManager::animations_;
std::unordered_map<std::string, std::shared_ptr<Sound>> ResourceManager::sounds_;
std::unordered_map<std::string, std::shared_ptr<Wave>> ResourceManager::waves_;
std::unordered_map<std::string, std::shared_ptr<Music>> ResourceManager::musics_;
std::unordered_map<std::string, std::shared_ptr<Font>> ResourceManager::fonts_;


std::shared_ptr<Model> ResourceManager::GetModelByIndex(int index)
{
	return std::shared_ptr<Model>();
}

std::shared_ptr<Texture2D> ResourceManager::GetTextureByVram(int x, int y)
{
	return std::shared_ptr<Texture2D>();
}

void ResourceManager::LoadGameDatabases(const int16_t LanguageID)
{
    ResourceManager::LoadTFile("./CD/COM/MO.T");
    ResourceManager::LoadTFile("./CD/COM/TALK" + std::to_string(LanguageID) + ".T");
    ResourceManager::LoadTFile("./CD/COM/VAB.T");
    ResourceManager::LoadTFile("./CD/COM/FDAT.T");
    ResourceManager::LoadTFile("./CD/COM/RTIM.T");
    ResourceManager::LoadTFile("./CD/COM/RTMD.T");
    ResourceManager::LoadTFile("./CD/COM/ITEM" + std::to_string(LanguageID) + ".T");
}

std::shared_ptr<TFile> ResourceManager::LoadTFile(const std::string& path)
{
    // Если архив уже загружен, возвращаем его
    auto it = tfiles_.find(path);
    if (it != tfiles_.end()) {
        return it->second;
    }
    printf("LoadTFile: load new..\n");
    // Иначе создаем новый, загружаем и кешируем
    auto tfile = std::make_shared<TFile>(path);
    tfiles_[path] = tfile;
    return tfile;
}

std::shared_ptr<TextureDB> ResourceManager::LoadKFTextures(const std::string& path, int index)
{
    // 0. Защита от дурака
    if (index < 0)
    {
        TraceLog(LOG_ERROR, "ResourceManager::LoadKFTextures index < 0 <%i>",index);
        return nullptr;
    }

    // 1. Формируем УНИКАЛЬНЫЙ ключ для кеша: "Путь_Индекс"
    // Пример: "TALK0.T_5"
    std::string cacheKey = path + "_" + std::to_string(index);

    // 2. Проверяем кеш готовых текстур
    auto it = kftexture_.find(cacheKey);
    if (it != kftexture_.end()) {
        return it->second;
    }

    // 3. Получаем сам архив (он тоже кешируется внутри LoadTFile)
    auto tfile = LoadTFile(path);
    if (!tfile) return nullptr;

    // Проверка границ
    if (static_cast<size_t>(index) >= tfile->getNumFiles()) {
        printf("ResourceManager: Index %d out of range for %s\n", index, path.c_str());
        return nullptr;
    }

    // 4. Получаем данные и ТИП файла
    ByteArray& fileData = tfile->getFile(static_cast<size_t>(index));
    FTYPE type = tfile->getEType(fileData);

    // 5. ВАЖНО: Если это не текстура, даже не пытаемся парсить!
    // Это предотвращает краши на звуковых файлах.
    if (type != FTYPE::TIM && type != FTYPE::RTIM) {
        // Можно раскомментировать для отладки, но обычно это просто шум
        // printf("File %d is not a texture (Type: %d). Skipping.\n", fileIndex, (int)type);
        return nullptr;
    }

    // 6. Создаем TextureDB
    // Мы передаем тип, чтобы конструктор знал, какой парсер использовать, 
    // или пусть конструктор сам определяет (см. ниже).
    auto textureDB = std::make_shared<TextureDB>(fileData);

    // 7. Проверяем, загрузилось ли хоть что-то
    if (textureDB->getTextureCount() == 0) {
        printf("WARNING: File %d identified as texture but parsing failed.\n", index);
        return nullptr;
    }
   
    // 8. Сохраняем в кеш и возвращаем
    kftexture_[cacheKey] = textureDB;
    return textureDB;
}

ByteArray& ResourceManager::GetFileFromT(const std::string& archivePath, size_t fileIndex)
{
    auto archive = LoadTFile(archivePath);
    return archive->getFile(fileIndex);
}

std::shared_ptr<Texture2D> ResourceManager::LoadTexture(const std::string& path, bool GetMipMap, TextureFilter filter, TextureWrap wrap)
{
    // 1. Проверяем кэш
    std::string key = path + "|" +
        std::to_string((int)filter) + "|" +
        (GetMipMap ? "1" : "0") + "|" +
        std::to_string((int)wrap);

    auto it = textures_.find(key);
    if (it != textures_.end()) {
        std::shared_ptr<Texture2D> locked = it->second;
        if (locked) return locked; // Вернули существующий
    }

    // 2. Загружаем, если нет в кэше
    // ВАЖНО: Выделяем память под структуру (new Texture2D)
    Texture2D tex = ::LoadTexture(path.c_str());
    if (tex.id == 0) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return nullptr;
    }
    if (GetMipMap)
        GenTextureMipmaps(&tex);

    SetTextureFilter(tex, filter);
    SetTextureWrap(tex, wrap);

    // 3. Создаем shared_ptr с КАСТОМНЫМ УДАЛИТЕЛЕМ
    std::shared_ptr<Texture2D> ptr(new Texture2D(tex), [](Texture2D* t) {
        std::cout << "Unloading Texture..." << std::endl;
        UnloadTexture(*t); // Raylib функция очистки
        delete t;          // Очистка памяти C++
        });

    // 4. Сохраняем в кэш
    textures_[key] = ptr;
    return ptr;
}

std::shared_ptr<Model> ResourceManager::LoadModel(const std::string& path)
{
    auto it = models_.find(path);
    if (it != models_.end())
    {
        return it->second;
        /* auto locked = it->second.lock();
         if (locked) return locked;*/
    }

    Model rawModel = ::LoadModel(path.c_str());

    if (rawModel.meshCount == 0) {
        TraceLog(LOG_WARNING, "RESOURCE: Failed to load model: %s", path.c_str());
        return nullptr;
    }

    std::shared_ptr<Model> ptr(new Model(rawModel), [](Model* m) {
        // Проверка на валидность указателя перед удалением
        if (m) {
            // Защита текстур
            if (m->materials) {
                for (int i = 0; i < m->materialCount; i++) {
                    m->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id = 1;
                }
            }

            // UnloadModel безопасно вызывать, только если m->meshes != nullptr
            if (m->meshCount > 0 && m->meshes != nullptr) {
                TraceLog(LOG_INFO, "RESOURCE: Unloading model VRAM...");
                UnloadModel(*m);
            }

            TraceLog(LOG_INFO, "RESOURCE: Deleting model struct...");
            delete m; // <-- Краш здесь означает, что кто-то другой (Тени) испортил память
        }
        });

    models_[path] = ptr; // Если models_ хранит weak_ptr, тут надо сделать .lock() или хранить shared_ptr
    return ptr;
}

std::shared_ptr<AnimationData> ResourceManager::LoadModelAnimation(const std::string& path)
{
    auto it = animations_.find(path);
    if (it != animations_.end()) {
        auto locked = it->second;
        if (locked) return locked;
    }

    int count = 0;
    ModelAnimation* anims = LoadModelAnimations(path.c_str(), &count);

    if (!anims) return nullptr;

    auto data = new AnimationData{ anims, count };

    TraceLog(LOG_INFO, "ResourceManager::LoadModelAnimation : anim count : %i\n", count);
    // Deleter не нужен, так как он прописан в деструкторе struct AnimationData
    //std::shared_ptr<AnimationData> ptr(data);
    std::shared_ptr<AnimationData> ptr(new AnimationData{ anims, count }, [](AnimationData* d) {
        if (d) {
            if (d->anims) {
                TraceLog(LOG_INFO, "RESOURCE: Unloading %i animations", d->count);
                ::UnloadModelAnimations(d->anims, d->count);
            }
            delete d;
        }
        });

    animations_[path] = ptr;
    return ptr;
}

std::shared_ptr<Sound> ResourceManager::LoadSound(const std::string& path)
{
    auto it = sounds_.find(path);
    if (it != sounds_.end()) {
        auto locked = it->second;
        if (locked) return locked;
    }
    Sound snd = ::LoadSound(path.c_str());
    std::shared_ptr<Sound> ptr(new Sound(snd), [](Sound* s) { UnloadSound(*s); delete s; });
    sounds_[path] = ptr;
    return ptr;
}

std::shared_ptr<Wave> ResourceManager::LoadWavs(const std::string& path)
{
    auto it = waves_.find(path);
    if (it != waves_.end()) {
        auto locked = it->second;
        if (locked) return locked;
    }
    Wave snd = ::LoadWave(path.c_str());

    std::shared_ptr<Wave> ptr(new Wave(snd), [](Wave* s) { UnloadWave(*s); delete s; });
    waves_[path] = ptr;
    return ptr;
}

std::shared_ptr<Music> ResourceManager::LoadMusic(const std::string& path)
{
    auto it = musics_.find(path);
    if (it != musics_.end()) {
        if (auto sp = it->second) {
            return sp;
        }
    }

    Music mus = ::LoadMusicStream(path.c_str());
    // Note: after loading stream, user must call UpdateMusicStream in game loop
    auto deleter = [](Music* m) {
        ::UnloadMusicStream(*m);
        delete m;
    };
    std::shared_ptr<Music> sp(new Music(mus), deleter);
    musics_[path] = sp;

    return sp;
}

std::shared_ptr<Font> ResourceManager::LoadFont(const std::string& path, int fontSize)
{
    // Формируем уникальный ключ: "data/font.ttf_32"
    std::string key = path + "_" + std::to_string(fontSize);

    auto it = fonts_.find(key);
    if (it != fonts_.end()) {
        if (auto sp = it->second) {
            return sp;
        }
    }

    Font fnt;
    bool isDefault = false;
    if (!FileExists(path.c_str()))
    {
        fnt = GetFontDefault();
        isDefault = true;
        TraceLog(LOG_ERROR, "Not found font path <%s> , fallback to default", path.c_str());
    }
    else
    {
        // Prepare codepoints array
        std::vector<int> cp;
        for (int c = 32; c < 127; ++c) cp.push_back(c);
        for (int c = 0x0400; c <= 0x0450; ++c) cp.push_back(c);
        int glyphCount = static_cast<int>(cp.size());
        auto cpData = std::make_unique<int[]>(glyphCount);
        std::copy(cp.begin(), cp.end(), cpData.get());


        fnt = ::LoadFontEx(path.c_str(), fontSize, cpData.get(), glyphCount);
    }


    // Check validity: fnt.texture.id? If 0, failed.
    auto deleter = [isDefault](Font* f) {
        if (!isDefault) {
            ::UnloadFont(*f); // Только для реально загруженных шрифтов
        }
        delete f;
    };
    std::shared_ptr<Font> sp(new Font(fnt), deleter);
    fonts_[key] = sp;

    return sp;
}

void ResourceManager::UnloadAll()
{
    textures_.clear();
    models_.clear();
    animations_.clear();
    sounds_.clear();
    waves_.clear();
    musics_.clear();
}
