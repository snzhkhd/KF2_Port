#pragma once
#include "types.h"
#include <string>
#include <map>

enum FTYPE
{
    TMD,
    TIM,
    VH,
    SEQ,
    VB,
    MAP1,
    MAP2,
    MAP3,
    RTIM,
    RTMD,
    MO,
    GAMEDB,
    DATA
};

//----------FILES----------//


struct TFile
{
public:
    explicit TFile(const std::string& filename);

    // Вместо QString используем std::string
    std::string getBaseFilename() const;
    std::string getFilename() const;

    // Возвращаем ссылку на вектор байт конкретного файла внутри архива
    ByteArray& getFile(size_t index);

    std::string getFiletype(const ByteArray& file) const;
    FTYPE getEType(const ByteArray& file) const;

    size_t getNumFiles() const;

    // Вместо QFile используем стандартный поток или путь
    void writeTo(const std::string& outPath) const;

private:
    void load(const ByteArray& tFileBlob);

    bool loaded = false;
    std::string fileName;
    std::string fullPath;
    // Вектор векторов: список всех под-файлов внутри .T архива
    std::vector<ByteArray> files;
    std::vector<uint32_t> fileOffsets;
    std::map<uint32_t, uint32_t> fileMap;
};