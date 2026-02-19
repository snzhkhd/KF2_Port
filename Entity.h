#pragma once
#include "object.h"


#pragma pack(push, 1)
struct Entity
{
    EntityMeshID MeshID;
    uint8_t FourOrForty;
    uint8_t field_0x2;
    uint16_t KnockbackResistance;
    uint16_t field_0x5;
    uint8_t Unknown_x07;
    uint8_t Unknown_x08;
    uint8_t field_0x9;
    uint8_t SpawnDistance;
    uint8_t DespawnDistance;
    int16_t SomethingX;
    int16_t SomethingY;
    int16_t SomethingZ;
    uint16_t Unknown_x12;
    uint16_t Unknown_x14;
    int16_t LookingAtMargin1;
    int16_t LookingAtMargin2;
    uint16_t HP;
    uint16_t Unknown_x1c;
    uint16_t ExperienceGain;
    uint16_t DefSlash;
    uint16_t DefChop;
    uint16_t DefStab;
    uint16_t DefHolyMagic;
    uint16_t DefFireMagic;
    uint16_t DefEarthMagic;
    uint16_t DefWindMagic;
    uint16_t DefWaterMagic;
    uint16_t GoldSomething;
    int16_t Scale;
    uint32_t UknBitField34;
    uint32_t SomePointers[16];
};
#pragma pack(pop)

struct EntityInstance
{
    uint8_t RespawnMode;
    uint8_t EntityClass;
    uint8_t NonRandomRotation;
    uint8_t WEXTilePos;
    uint8_t NSYTilePos;
    uint8_t RespawnChance;
    ObjectID DroppedItem : 8;
    uint8_t Layer;
    uint16_t ZRotation;
    int16_t FineWEXPos;
    int16_t FineNSYPos;
    int16_t FineZPos;
};