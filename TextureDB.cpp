#include "TextureDB.h"
#include "utilities.h"
#include <stdexcept>
#include <iostream>

static uint16_t readU16(const ByteArray& data, size_t& pos) {
    // Проверка границ (безопасность)
    if (pos + 2 > data.size()) {
        throw std::out_of_range("readU16: out of bounds");
    }

    uint16_t val = data[pos] | (data[pos + 1] << 8);
    pos += 2;
    return val;
}

static uint32_t readU32(const ByteArray& data, size_t& pos) {
    // Проверка границ
    if (pos + 4 > data.size()) {
        throw std::out_of_range("readU32: out of bounds");
    }

    uint32_t val = data[pos] |
        (data[pos + 1] << 8) |
        (data[pos + 2] << 16) |
        (data[pos + 3] << 24);
    pos += 4;
    return val;
}

// === Запись ===

static void writeU16(ByteArray& out, uint16_t val) {
    out.push_back(val & 0xFF);
    out.push_back((val >> 8) & 0xFF);
}

static void writeU32(ByteArray& out, uint32_t val) {
    out.push_back(val & 0xFF);
    out.push_back((val >> 8) & 0xFF);
    out.push_back((val >> 16) & 0xFF);
    out.push_back((val >> 24) & 0xFF);
}

TextureDB::TextureDB(const std::vector<uint8_t>& data)
{
    // Вместо QDataStream будем передавать данные в специализированные методы
    if (Utilities::fileIsTIM(data))
    {
        type = TexDBType::TIM;
        loadTIM(data);
    }
    else if (Utilities::fileIsRTIM(data))
    {
        type = TexDBType::RTIM;
        loadRTIM(data);
    }
}

TextureDB::~TextureDB() {
    // Освобождаем Image из RayLib, если они были загружены
    for (auto& tex : textures) {
        if (tex.image.data != nullptr) {
            UnloadImage(tex.image);
        }
    }
}



Point TextureDB::getFramebufferCoordinate(size_t textureIndex)
{
    if (textureIndex >= textures.size()) return Point{ -999, -999 };

    return Point{ textures[textureIndex].frameBufferX, textures[textureIndex].frameBufferY };
}

TextureDB::KFTexture& TextureDB::getTexture(size_t textureIndex)
{
    if (textureIndex >= textures.size()) {
        throw std::out_of_range("TextureDB: Index out of range");
    }
    return textures[textureIndex];
}

void TextureDB::loadTIM(const ByteArray& data)
{
    size_t pos = 0;

    // Цикл работает, пока в буфере есть достаточно данных для заголовка (8 байт: ID + Flags)
    while (pos + 8 <= data.size())
    {
        // Сохраняем позицию перед чтением ID, чтобы проверить валидность
        size_t startPos = pos;

        // 1. Проверка магического числа (ID)
        // readU32 сдвигает pos на 4
        uint32_t id = readU32(data, pos);

        // Если ID не 0x10, значит это не TIM или это паддинг (нули) в конце файла
        if (id != 0x10) {
            // Можно вывести лог для отладки, если данные не просто нули
            // std::cout << "End of TIM chain or padding detected at " << startPos << std::endl;
            break;
        }

        // Создаем новую текстуру
        textures.emplace_back();
        KFTexture& tex = textures.back();

        // 2. Чтение флага режима
        uint32_t flag = readU32(data, pos);
        tex.pMode = static_cast<PixelMode>(flag & 0x07);
        tex.hasClut = static_cast<bool>((flag >> 3) & 0x01);

        bool clutOk = false;

        // 3. Чтение CLUT (если есть)
        if (tex.hasClut) {
            clutOk = parseCLUT(&data, pos, tex);
        }

        // 4. Чтение пикселей
        // Если палитра была нужна, но не прочиталась корректно — текстура битая
        if ((tex.hasClut && clutOk) || !tex.hasClut) {
            parsePixelData(&data, pos, tex);
        }
        else {
            // Если что-то пошло не так, удаляем последнюю созданную пустышку и выходим
            std::cerr << "TextureDB: Failed to load TIM chunk at " << startPos << std::endl;
            textures.pop_back();
            break;
        }
    }
}

void TextureDB::loadRTIM(const ByteArray& data)
{
    size_t pos = 0;

    // RTIM файлы в KF часто содержат несколько текстур подряд
    while (pos < data.size())
    {
        textures.emplace_back();
        KFTexture& tex = textures.back();

        // В RTIM палитра обязательна и идет первой
        if (!parseCLUT(&data, pos, tex)) {
            textures.pop_back();
            break;
        }

        parsePixelData(&data, pos, tex);
    }
}

// Вспомогательная функция для перевода 16-бит цвета PS1 (BGR555) в RayLib Color
Color TextureDB::PsxColorToRaylib(uint16_t psxColor)
{
    // Формат PS1: 1 бит прозрачности, 5 бит Blue, 5 бит Green, 5 бит Red
    // xBBBBBGGGGGRRRRR
    uint8_t r = (psxColor & 0x1F) << 3;
    uint8_t g = ((psxColor >> 5) & 0x1F) << 3;
    uint8_t b = ((psxColor >> 10) & 0x1F) << 3;

    // Бит 15 на PS1 — это STP (Semi-Transparency). 
    // Если он 0 и цвет 0,0,0 — это полная прозрачность.
    uint8_t a = 255;
    if (psxColor == 0) a = 0;

    return Color{ r, g, b, a };
}

/*
   ПРИМЕЧАНИЕ ПО replaceTexture:
   Оригинальный код использует libimagequant для конвертации современных картинок
   обратно в 4-битные палитры PS1.
   Если ты делаешь только плеер (порт), эта функция тебе не нужна.
   Если ты делаешь редактор, тебе нужно будет подключить libimagequant.
*/
void TextureDB::replaceTexture(Image& newTexture, size_t textureIndex)
{
    // TODO: Реализовать, если понадобится вставлять свои текстуры в .T архивы
    std::cout << "Texture replacement is not implemented yet." << std::endl;
}

bool TextureDB::appendFile(const ByteArray& fileData)
{
    // Пытаемся определить тип по данным (простая эвристика)
    // TIM начинается с 0x10, RTIM обычно идет без заголовка, но мы попробуем оба

    size_t initialCount = textures.size();
    size_t pos = 0;
    uint32_t id = readU32(fileData, pos); // Читаем первые 4 байта

    if (id == 0x10) {
        // Это TIM
        loadTIM(fileData);
    }
    else {
        // Пробуем как RTIM (сырые данные)
        loadRTIM(fileData);
    }

    // Если размер вектора увеличился, значит что-то загрузилось
    return textures.size() > initialCount;
}


std::vector<uint16_t> TextureDB::KFTexture::getCLUTEntries() const {
    std::vector<uint16_t> entries;
    for (const auto& color : clutColorTable) {
        // Конвертируем RGBA (8888) обратно в BGR (555)
        uint16_t r = (color.r >> 3) & 0x1F;
        uint16_t g = (color.g >> 3) & 0x1F;
        uint16_t b = (color.b >> 3) & 0x1F;
        uint16_t stp = (color.a > 0 && color.a < 255) ? 0x8000 : 0; // Бит полупрозрачности

        entries.push_back(r | (g << 5) | (b << 10) | stp);
    }
    return entries;
}


bool TextureDB::parseCLUT(const ByteArray* data, size_t& pos, KFTexture& target)
{
    // 1. Читаем размер (только если это не RTIM)
    if (type != TexDBType::RTIM) {
        target.clutSize = readU32(*data, pos);
    }

    // 2. Читаем основные параметры
    target.clutVramX = readU16(*data, pos);
    target.clutVramY = readU16(*data, pos);
    target.clutWidth = readU16(*data, pos);
    target.clutHeight = readU16(*data, pos);

    // 3. Специфика RTIM: проверка дубликата заголовка
    if (type == TexDBType::RTIM)
    {
        uint16_t dupX = readU16(*data, pos);
        uint16_t dupY = readU16(*data, pos);
        uint16_t dupW = readU16(*data, pos);
        uint16_t dupH = readU16(*data, pos);

        // Проверка 1: Дубликаты должны совпадать с оригиналом
        if (target.clutVramX != dupX || target.clutVramY != dupY ||
            target.clutWidth != dupW || target.clutHeight != dupH)
        {
            std::cerr << "TextureDB: Invalid RTIM CLUT Header: Dupes don't match." << std::endl;
            return false;
        }

        // Проверка 2: Эвристика для обнаружения "битых" или пустых секций (все значения равны)
        // В King's Field файлы иногда выравниваются мусором или нулями.
        if (target.clutVramX == target.clutVramY && target.clutVramY == target.clutWidth
            && target.clutWidth == target.clutHeight)
        {
            return false;
        }
    }

    // 4. Чтение цветов
    uint32_t clutAmount = target.clutWidth * target.clutHeight;
    target.clutColorTable.clear();
    target.clutColorTable.reserve(clutAmount);

    for (uint32_t i = 0; i < clutAmount; ++i)
    {
        uint16_t rawEntry = readU16(*data, pos);

        // Конвертация 15-бит BGR в 32-бит RGBA
        // (val & 0x1F) << 3  равносильно  (val & 31) * 8
        uint8_t r = (rawEntry & 0x1F) << 3;
        uint8_t g = ((rawEntry >> 5) & 0x1F) << 3;
        uint8_t b = ((rawEntry >> 10) & 0x1F) << 3;
        bool stp = (rawEntry & 0x8000) != 0; // Бит 15 (Semi-Transparency Processing)

        // Логика прозрачности King's Field (портировано из KFModTool)
        uint8_t a = 255; // По умолчанию непрозрачный

        if (!stp && r == 0 && g == 0 && b == 0) {
            // STP=0 и Цвет=Черный -> Полная прозрачность
            a = 0;
        }
        // В оригинальном коде есть комментарий, что KF не использует полупрозрачность (127),
        // поэтому остальные случаи (STP=1) мы считаем полностью непрозрачными (255).

        target.clutColorTable.push_back(Color{ r, g, b, a });
    }

    // В RayLib Image не хранит таблицу цветов отдельно, 
    // она применяется сразу при создании пикселей в readPixelData.
    // Но мы храним её в структуре Texture, чтобы знать, какие цвета использовать.

    return true;
}


void TextureDB::parsePixelData(const ByteArray* data, size_t& pos, KFTexture& target)
{
    // 1. Читаем заголовок
    if (type != TexDBType::RTIM) {
        target.pxDataSize = readU32(*data, pos);
    }

    target.pxVramX = readU16(*data, pos);
    target.pxVramY = readU16(*data, pos);
    target.pxWidth = readU16(*data, pos);
    target.pxHeight = readU16(*data, pos);

    // 2. Пропускаем дубликат заголовка (для RTIM)
    if (type == TexDBType::RTIM) {
        pos += 8; // 4 слова по 2 байта
    }

    // 3. Корректируем ширину (PS1 хранит ширину в 16-битных словах)
    if (target.pMode == PixelMode::CLUT4Bit) {
        target.pxWidth *= 4; // 1 слово = 4 пикселя (по 4 бита)
    }
    else if (target.pMode == PixelMode::CLUT8Bit) {
        target.pxWidth *= 2; // 1 слово = 2 пикселя (по 8 бит)
    }

    // Сохраняем координаты фреймбуфера
    target.frameBufferX = target.pxVramX;
    if (target.pMode == PixelMode::CLUT4Bit) target.frameBufferX *= 4;
    else if (target.pMode == PixelMode::CLUT8Bit) target.frameBufferX *= 2;

    target.frameBufferY = target.pxVramY;

    // 4. Подготовка буфера изображения для RayLib (RGBA8888)
    int totalPixels = target.pxWidth * target.pxHeight;
    int dataSize = totalPixels * 4; // 4 байта на пиксель (R,G,B,A)

    // Выделяем память (используем calloc, чтобы занулить альфу по умолчанию)
    unsigned char* pixels = (unsigned char*)calloc(dataSize, 1);

    // Лямбда для удобной установки пикселя
    auto setPixel = [&](int idx, Color c) {
        if (idx >= totalPixels) return;
        int offset = idx * 4;
        pixels[offset] = c.r;
        pixels[offset + 1] = c.g;
        pixels[offset + 2] = c.b;
        pixels[offset + 3] = c.a;
    };

    int curPixel = 0;

    // 5. Главный цикл декодирования
    // Мы читаем данные блоками по 16 бит (uint16_t), как это делает PS1
    while (curPixel < totalPixels && pos < data->size()) // Добавил проверку pos для безопасности
    {
        uint16_t block = readU16(*data, pos);

        switch (target.pMode)
        {
        case PixelMode::CLUT4Bit:
        {
            // Извлекаем 4 индекса (по 4 бита)
            // Порядок важен: младшие биты - первые пиксели
            uint8_t idx0 = block & 0x0F;
            uint8_t idx1 = (block >> 4) & 0x0F;
            uint8_t idx2 = (block >> 8) & 0x0F;
            uint8_t idx3 = (block >> 12) & 0x0F;

            // Берем цвет из палитры (clutColorTable) и пишем в буфер
            if (!target.clutColorTable.empty()) {
                setPixel(curPixel++, target.clutColorTable[idx0]);
                setPixel(curPixel++, target.clutColorTable[idx1]);
                setPixel(curPixel++, target.clutColorTable[idx2]);
                setPixel(curPixel++, target.clutColorTable[idx3]);
            }
            else {
                curPixel += 4; // Если палитры нет, пропускаем (черный/прозрачный)
            }
            break;
        }
        case PixelMode::CLUT8Bit:
        {
            // Извлекаем 2 индекса (по 8 бит)
            uint8_t idx0 = block & 0xFF;
            uint8_t idx1 = (block >> 8) & 0xFF;

            if (!target.clutColorTable.empty()) {
                setPixel(curPixel++, target.clutColorTable[idx0]);
                setPixel(curPixel++, target.clutColorTable[idx1]);
            }
            else {
                curPixel += 2;
            }
            break;
        }
        case PixelMode::Direct15Bit:
        {
            // Прямой цвет (без палитры)
            // Используем нашу функцию конвертации
            Color c = PsxColorToRaylib(block);
            setPixel(curPixel++, c);
            break;
        }
        default:
            std::cerr << "Unimplemented pixel mode" << std::endl;
            curPixel++; // Избегаем бесконечного цикла
            break;
        }
    }

    // 6. Формируем объект RayLib Image
    target.image.data = pixels;
    target.image.width = target.pxWidth;
    target.image.height = target.pxHeight;
    target.image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    target.image.mipmaps = 1;
}