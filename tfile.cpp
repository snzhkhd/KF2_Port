#include "tfile.h"
#include "utilities.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace fs = std::filesystem;

static uint16_t readU16LE(const ByteArray& data, size_t& pos) {
    uint16_t val = data[pos] | (data[pos + 1] << 8);
    pos += 2;
    return val;
}

static void writeU16LE(std::vector<uint8_t>& data, uint16_t val) {
    data.push_back(static_cast<uint8_t>(val & 0xFF));
    data.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}


TFile::TFile(const std::string& filename)
{
    fs::path p(filename);

    // 1. Извлекаем имя файла (аналог filename.mid(lastIndexOf...))
    this->fileName = p.filename().string();
    this->fullPath = filename;

    // 2. Читаем файл в буфер (аналог QFile::readAll)
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        // В порте лучше использовать исключения или лог RayLib
        std::cerr << "Failed to open T-File: " << filename << std::endl;
        return;
    }

    // Определяем размер и читаем
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    ByteArray buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        load(buffer); // Вызываем парсинг внутренней структуры
        loaded = true;
    }
}

std::string TFile::getBaseFilename() const
{
    fs::path p(fileName);
    return p.stem().string();
}

std::string TFile::getFilename() const
{
    return fileName;
}

ByteArray& TFile::getFile(size_t index)
{
    // Проверка границ (аналог fatalError)
    if (index >= files.size()) {
        throw std::out_of_range("TFile: getFile called for out-of-bounds index " + std::to_string(index));
    }

    return files.at(index);
}


std::string TFile::getFiletype(const ByteArray& file) const
{
    if (Utilities::fileIsTMD(file)) return "TMD";
    if (Utilities::fileIsTIM(file)) return "TIM";
    if (Utilities::fileIsVH(file)) return "VH";
    if (Utilities::fileIsSEQ(file)) return "SEQ";
    if (Utilities::fileIsVB(file)) return "VB";
    if (Utilities::fileIsMAP1(file)) return "MAP1";
    if (Utilities::fileIsMAP2(file)) return "MAP2";
    if (Utilities::fileIsMAP3(file)) return "MAP3";
    if (Utilities::fileIsRTIM(file)) return "RTIM";
    if (Utilities::fileIsRTMD(file)) return "RTMD";
    if (Utilities::fileIsMO(file)) return "MO";
    if (Utilities::fileIsGameDB(file)) return "GAMEDB";

    return "DATA";
}

FTYPE TFile::getEType(const ByteArray& file) const
{
    if (Utilities::fileIsTMD(file)) return FTYPE::TMD;
    if (Utilities::fileIsTIM(file)) return FTYPE::TIM;
    if (Utilities::fileIsVH(file)) return FTYPE::VH;
    if (Utilities::fileIsSEQ(file)) return FTYPE::SEQ;
    if (Utilities::fileIsVB(file)) return FTYPE::VB;
    if (Utilities::fileIsMAP1(file)) return FTYPE::MAP1;
    if (Utilities::fileIsMAP2(file)) return FTYPE::MAP2;
    if (Utilities::fileIsMAP3(file)) return FTYPE::MAP3;
    if (Utilities::fileIsRTIM(file)) return FTYPE::RTIM;
    if (Utilities::fileIsRTMD(file)) return FTYPE::RTMD;
    if (Utilities::fileIsMO(file)) return FTYPE::MO;
    if (Utilities::fileIsGameDB(file)) return FTYPE::GAMEDB;

    return FTYPE::DATA;
}

size_t TFile::getNumFiles() const
{
    return files.size();
}

void TFile::writeTo(const std::string& outPath) const
{
    std::vector<uint8_t> dataBlob;
    std::vector<uint16_t> newTrueOffsets;

    // 1. Собираем данные файлов в один большой блок с выравниванием по 2048 байт
    for (const auto& file : files)
    {
        // Вычисляем текущее смещение в секторах (начинаем с сектора 1, т.к. сектор 0 - заголовок)
        newTrueOffsets.push_back(static_cast<uint16_t>((dataBlob.size() + 2048) / 2048));

        // Добавляем данные файла
        dataBlob.insert(dataBlob.end(), file.begin(), file.end());

        // Выравнивание (Padding) до 2048 байт
        size_t padding = 2048 - (file.size() % 2048);
        if (padding != 2048) {
            dataBlob.insert(dataBlob.end(), padding, 0x00);
        }
    }

    // Добавляем финальное смещение (EOF)
    newTrueOffsets.push_back(static_cast<uint16_t>((dataBlob.size() + 2048) / 2048));

    // 2. Генерируем итоговый файл
    ByteArray finalFile;
    finalFile.reserve(dataBlob.size() + 2048);

    // Записываем количество файлов (fileMap.size() - 1)
    uint16_t nFiles = static_cast<uint16_t>(fileMap.size() - 1);
    finalFile.push_back(nFiles & 0xFF);
    finalFile.push_back((nFiles >> 8) & 0xFF);

    // Записываем таблицу указателей
    for (size_t i = 0; i < fileMap.size(); ++i) {
        uint16_t offset = newTrueOffsets.at(fileMap.at(i));
        finalFile.push_back(offset & 0xFF);
        finalFile.push_back((offset >> 8) & 0xFF);
    }

    // Заполняем пустое место в первом секторе нулями до 2048 байт
    while (finalFile.size() < 2048) {
        finalFile.push_back(0);
    }

    // Добавляем все данные после заголовка
    finalFile.insert(finalFile.end(), dataBlob.begin(), dataBlob.end());

    // 3. Записываем на диск
    std::ofstream outFile(outPath, std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(finalFile.data()), finalFile.size());
}

void TFile::load(const ByteArray& tFileBlob)
{
    if (tFileBlob.empty()) return;

    size_t pos = 0;
    uint32_t trueFileNum = 0;

    // 1. Читаем количество файлов (nFiles)
    uint16_t nFiles = readU16LE(tFileBlob, pos);

    fileOffsets.clear();
    fileOffsets.reserve(nFiles + 1);
    fileMap.clear();

    // 2. Читаем таблицу смещений
    // Мы идем до nFiles включительно, чтобы захватить EOF offset
    for (unsigned int i = 0; i <= nFiles; i++)
    {
        uint16_t offset = readU16LE(tFileBlob, pos);
        uint32_t convertedOffset = offset * 2048; // Смещение в байтах

        // Проверяем на дубликаты (несколько индексов могут указывать на одни данные)
        if (fileOffsets.empty() || fileOffsets.back() != convertedOffset)
        {
            fileOffsets.push_back(convertedOffset);
            trueFileNum++;
        }

        // Регистрируем соответствие индекса файла и номера уникального блока данных
        fileMap[i] = trueFileNum - 1;
    }

    // 3. Извлекаем данные файлов на основе смещений
    files.clear();
    files.reserve(fileOffsets.size() - 1);

    for (size_t i = 1; i < fileOffsets.size(); ++i)
    {
        uint32_t start = fileOffsets[i - 1];
        uint32_t end = fileOffsets[i];
        uint32_t size = end - start;

        if (start + size <= tFileBlob.size()) {
            // Создаем копию куска байт (аналог tFileBlob.mid)
            ByteArray fileData(tFileBlob.begin() + start, tFileBlob.begin() + end);
            files.push_back(std::move(fileData));
        }
    }

    loaded = true;
}
