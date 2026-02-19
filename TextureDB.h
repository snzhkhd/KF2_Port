#pragma once

#include "types.h"
#include <cstdint>
#include <string>
#include "raylib.h" // Используем типы RayLib для цвета и изображений

enum class TexDBType {
    RTIM,
    TIM
};
enum class PixelMode {
    CLUT4Bit = 0,
    CLUT8Bit = 1,
    Direct15Bit = 2,
    Direct24Bit = 3,
    Mixed = 4
};

class TextureDB
{
public:
    
    struct KFTexture {
        // Метаданные
        PixelMode pMode;
        bool hasClut = true;
        int frameBufferX = 0;
        int frameBufferY = 0;

        // Данные палитры (CLUT)
        uint32_t clutSize = 0;
        uint16_t clutVramX = 0;
        uint16_t clutVramY = 0;
        uint16_t clutWidth = 0;
        uint16_t clutHeight = 0;
        std::vector<Color> clutColorTable; // RayLib Color (RGBA)

        // Данные пикселей
        uint32_t pxDataSize = 0;
        uint16_t pxVramX = 0;
        uint16_t pxVramY = 0;
        uint16_t pxWidth = 0;
        uint16_t pxHeight = 0;

        // Итоговое изображение для RayLib
        Image image;

        // Вспомогательная функция для получения "сырой" палитры в формате PS1
        std::vector<uint16_t> getCLUTEntries() const;
    };

    TextureDB() = default;

    // Конструктор принимает вектор байт (один файл из .T архива)
    explicit TextureDB(const ByteArray& data);
    ~TextureDB();

    // Интерфейс доступа
    size_t getTextureCount() const { return textures.size(); }
    KFTexture& getTexture(size_t index);



    Point getFramebufferCoordinate(size_t textureIndex);
    void replaceTexture(Image& newTexture, size_t textureIndex);

    bool appendFile(const ByteArray& fileData);
    // Получить все текстуры
    const std::vector<KFTexture>& getAllTextures() const { return textures; }
private:
    void loadRTIM(const ByteArray& data);
    void loadTIM(const ByteArray& data);

    // Нам понадобятся низкоуровневые парсеры
    bool parseCLUT(const ByteArray* data, size_t& pos, KFTexture& target);
    void parsePixelData(const ByteArray* data, size_t& pos, KFTexture& target);

    // Конвертер из PS1 BGR555 в RayLib Color
    static Color PsxColorToRaylib(uint16_t psxColor);

    std::vector<KFTexture> textures;
    TexDBType type;
};