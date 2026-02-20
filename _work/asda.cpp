struct Entity {
    // === Заголовок (байты 0-15) ===
    uint8_t  type;           // +0x00: тип сущности (a2)
    uint8_t  effect_id;      // +0x01: визуальный эффект (a3)
    uint8_t  anim_frame_a;   // +0x02: кадр анимации A
    uint8_t  anim_frame_b;   // +0x03: кадр анимации B
    uint8_t  render_flags;   // +0x04: 0x80=textured, 0x01=blend...
    uint8_t  update_counter; // +0x05: счётчик кадров
    uint8_t  entity_flags;   // +0x06: флаги (a1)
    uint8_t  state_timer;    // +0x07: таймер состояния/кулдауна
    uint8_t  render_mode;    // +0x08: режим отрисовки
    uint8_t  blend_mode;     // +0x09: режим смешивания
    uint8_t  unused_a;       // +0x0A
    uint8_t  update_freq;    // +0x0B: частота обновления (1,3,8)
    uint8_t  collision_type; // +0x0C: тип коллизии (68=снаряд, 73=игрок...)
    uint8_t  is_semitrans;   // +0x0D: полупрозрачность (0/1)
    uint16_t lifetime;       // +0x0E: время жизни в кадрах
    uint16_t scale_factor;   // +0x10: масштаб (4096=1.0 fixed-point)
    
    // === Позиция и движение (байты 20-43) ===
    int32_t  pos_x;          // +0x14: позиция X
    int32_t  pos_y;          // +0x18: позиция Y (высота)
    int32_t  pos_z;          // +0x1C: позиция Z
    int32_t  vel_x;          // +0x20: скорость X
    int32_t  vel_y;          // +0x24: скорость Y
    int32_t  vel_z;          // +0x28: скорость Z
    int16_t  scale_x;        // +0x2C: масштаб по X
    int16_t  scale_y;        // +0x2E: масштаб по Y
    int16_t  scale_z;        // +0x30: масштаб по Z
    
    // === Параметры (байты 44-71) ===
    int16_t  damage;         // +0x32: урон
    int16_t  radius;         // +0x34: радиус эффекта
    int16_t  angle_tolerance;// +0x36: допуск угла атаки
    int16_t  element_mask;   // +0x38: битовая маска стихий
    int16_t  target_id;      // +0x3A: ID цели
    int16_t  sound_id;       // +0x3C: звук при создании
    int16_t  sound_volume;   // +0x3E: громкость звука
    // ... ещё поля для анимации, частиц, триггеров ...
};

struct ParticleEntity {        // sizeof=72 (0x48), align=1
    // === Заголовок (0-11) ===
    uint8_t  entity_type;      // +0x00: тип (0x10=стрела, 0x12=магия...)
    uint8_t  effect_id;        // +0x01: визуальный эффект
    uint8_t  anim_frame_a;     // +0x02: кадр анимации A
    uint8_t  anim_frame_b;     // +0x03: кадр анимации B
    uint8_t  render_flags;     // +0x04: 0x80=textured
    uint8_t  update_counter;   // +0x05: счётчик кадров
    uint8_t  entity_flags;     // +0x06: флаги активности
    uint8_t  state_timer;      // +0x07: таймер состояния
    uint8_t  anim_state;       // +0x08: 5=playing, 0=stop...
    uint8_t  blend_mode;       // +0x09: режим смешивания
    uint8_t  unused_a;         // +0x0A
    uint8_t  update_freq;      // +0x0B: частота обновления
    
    // === Коллизия и время жизни (12-19) ===
    uint8_t  collision_type;   // +0x0C: 67=снаряд, 73=игрок...
    uint8_t  is_semitrans;     // +0x0D: полупрозрачность
    uint16_t lifetime;         // +0x0E: время жизни (кадры)
    uint16_t scale_factor;     // +0x10: масштаб (4096=1.0)
    uint16_t unused_12;        // +0x12
    
    // === Позиция (20-31) === ← здесь начинается вектор для звука!
    int32_t  pos_x;            // +0x14
    int32_t  pos_y;            // +0x18
    int32_t  pos_z;            // +0x1C
    
    // === Скорость (32-43) ===
    int32_t  vel_x;            // +0x20
    int32_t  vel_y;            // +0x24
    int32_t  vel_z;            // +0x28
    
    // === Параметры (44-55) ===
    int16_t  speed_x;          // +0x2C
    int16_t  speed_y;          // +0x2E
    int16_t  speed_z;          // +0x30
    int16_t  damage;           // +0x32
    int16_t  radius;           // +0x34
    int16_t  angle_tolerance;  // +0x36
    
    // === Цели и звуки (56-71) ===
    int16_t  target_id;        // +0x38: ID цели
    int16_t  sound_id;         // +0x3A: звук при создании
    int16_t  sound_volume;     // +0x3C: громкость
    uint8_t  element_mask;     // +0x3E: стихии
    uint8_t  padding[5];       // +0x3F: выравнивание до 72
};