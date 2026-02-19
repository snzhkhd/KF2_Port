#pragma once
#include "structs.h"

/*!
 * \brief Structure for the object class declarations in the KF2 game database.
 */
struct ObjectClass
{
    ObjectClassType ClassType;
    uint8_t TransformType;
    uint8_t field_0x2;
    uint8_t field_0x3;
    uint16_t CollisionRadius;
    uint8_t field_0x6;
    uint8_t field_0x7;
    uint8_t field_0x8;
    uint8_t field_0x9;
    uint8_t field_0xa;
    uint8_t field_0xb;
    uint8_t field_0xc;
    uint8_t field_0xd;
    uint8_t field_0xe;
    uint8_t field_0xf;
    uint8_t field_0x10;
    uint8_t field_0x11;
    uint8_t field_0x12;
    uint8_t field_0x13;
    uint8_t field_0x14;
    uint8_t field_0x15;
    uint8_t field_0x16;
    uint8_t field_0x17;
};


struct ObjectInstance
{
    uint8_t TileLayer;
    uint8_t WEXTilePos;
    uint8_t NSYTilePos;
    uint8_t field_0x3;
    ObjectID ID : 16;
    uint16_t ZRotation;
    int16_t FineWEXPos;
    int16_t FineNSYPos;
    int16_t FineZPos;
    uint8_t Flags[10];
};