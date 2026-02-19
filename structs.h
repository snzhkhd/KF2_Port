#pragma once
#include "enums.h"
#include <array>

//----------SYSTEM----------//

struct CdStreamRequest {
    uint8_t  type;           // +0: 64 = CD_STREAM type
    uint8_t  active_flag;    // +1: 1 = processing
    uint8_t  pad[2];
    uint32_t dst_ptr;        // +4: куда копировать данные
    uint32_t src_info;       // +8: источник (CD sector info)
    uint16_t* sector_list;   // +12: массив секторов для загрузки
    uint32_t chunk_size;     // +16: размер текущего трансфера
    uint16_t sector_start;   // +24: первый сектор
    uint16_t sector_pos;     // +26: текущая позиция в загрузке
    uint16_t sectors_per_chunk; // +28: сколько секторов за один DMA
    uint16_t sectors_remaining; // +30: сколько осталось
    uint16_t timeout_counter;   // +32: watchdog для GPU/DMA
    uint16_t progress;          // +34: прогресс в чанке
    uint8_t  request_pending;   // +36: флаг новой задачи
};



struct FileDescriptor
{
    uint32_t ram_buffer;
    uint32_t sector_low;
    uint32_t sector_high;
};

struct MenuUI {
    uint16_t pos_x;           // 0x00 = 17
    uint16_t pos_y;           // 0x02 = 19
    uint8_t  frame_data[24];  // 0x04 - данные рамки/фона
    uint8_t  total_items;     // 0x1C (28) = 42
    uint8_t  selected_idx;    // 0x1D (29) = -97 (начальное)
    uint8_t  visible_items;   // 0x1F (31) = 4
    uint8_t  scroll_offset;   // 0x20 (32) = 0
    uint8_t  current_item;    // 0x21 (33) = 0
    uint8_t  viewport_top;    // 0x22 (34) = 0
    uint8_t  viewport_bottom; // 0x23 (35) = 20
};


//----------OTHER----------//
struct Player
{
    Vector3 PlayerPos;

    EntityStateID State;

    char startLayer = 1;
};

struct PsxRect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};


struct PlayerLevelData
{
    uint16_t BaseHP;
    uint16_t BaseMP;
    uint16_t StrPowerDiff;
    uint16_t MagPowerDiff;
    uint32_t ExpForNextLevel;

};


struct SpellEntry 
{
    uint16_t spell_id;      // [0] ID заклинания
    uint16_t damage;        // [1] базовый урон
    uint16_t range;         // [2] дальность
    uint16_t duration;      // [3] длительность эффекта
    uint16_t mana_cost;     // [11] ← стоимость в MP!
};

struct Projectile
{
    // 0x00
    uint8_t type;           // 0
    uint8_t flags;          // 1 (Set to 0x80 usually)
    uint8_t parentIndex;    // 2 (Set to -1)
    uint8_t padding1;       // 3

    // 0x04
    uint8_t modelId;        // 4 (96, 97, 98...) - Визуальная модель
    uint8_t subState;       // 5 (Set to -1)
    uint16_t status;        // 6 (255 = Free, otherwise Active)

    // 0x08
    uint16_t animTimer;     // 8
    uint16_t lifeTimer;     // 10 (Set to 0)

    // 0x0C
    uint16_t unk_0C;        // 12
    uint16_t unk_0E;        // 14 (Set to 0)
    uint16_t unk_10;        // 16 (Set to 0)
    uint16_t padding2;      // 18

    // 0x14 - Position (World Space)
    int32_t x;              // 20
    int32_t y;              // 24
    int32_t z;              // 28

    // 0x20
    int32_t unk_20;         // 32

    // 0x24 - Rotation / Velocity Vector?
    int16_t rotX;           // 36 (Set to 0)
    int16_t rotY;           // 38 (Random >> 3 in Spawn) - YAW угол
    int16_t rotZ;           // 40 (Set to 0)

    // 0x2A
    int16_t speed;          // 42 (Set to 1024 or 0 in Spawn)

    // 0x2C - Scale (Fixed point 4096 = 1.0)
    int16_t scaleX;         // 44 (4096)
    int16_t scaleY;         // 46 (4096)
    int16_t scaleZ;         // 48 (4096)

    // ... (пропуск до 60) ...
    // 0x3C
    uint16_t creationTime;  // 60 (Timestamp for LRU)

    // 0x3E
    uint16_t ownerId;       // 62 (Set to 0)

    // Total 68 bytes
};


