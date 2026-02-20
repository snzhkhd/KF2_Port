import ida_enum
import idaapi
import os
import re

# Получаем путь к файлу
script_dir = os.path.dirname(os.path.abspath(__file__))
file_path = os.path.join(script_dir, "ida_enum.h")

print(f"Читаем файл: {file_path}")

# Удаляем старый enum
eid = ida_enum.get_enum("EMessage")
if eid != idaapi.BADNODE:
    ida_enum.del_enum(eid)

# Создаем новый (0x10000 = decimal display)
eid = ida_enum.add_enum(0, "EMessage", 0x10000)

count = 0
items_added = []

# Читаем файл
with open(file_path, "r", encoding='utf-8') as f:
    content = f.read()

# Регулярка для C-style enum: ItemName = 123,
pattern = r'(\w+)\s*=\s*(\d+)'

for match in re.finditer(pattern, content):
    name = match.group(1)
    value = int(match.group(2))
    
    # Пропускаем служебные имена
    if name in ['enum', 'struct', 'class', 'EMessage']:
        continue
    
    result = ida_enum.add_enum_member(eid, name, value, 0xFFFFFFFF)
    if result != idaapi.BADNODE:
        count += 1
        items_added.append(f"{value} {name}")
    else:
        print(f"Не удалось добавить: {name}")

print(f"\n✅ Добавлено {count} элементов")
print("\nПример добавленных:")
for item in items_added[:5]:
    print(f"  {item}")
print("  ...")