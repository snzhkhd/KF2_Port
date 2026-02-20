file_path = r"F:\RayLib\KF2_Port\_work\items.txt"
with open(file_path, "r", encoding='utf-8') as f:
    content = f.read()
    print(content[:500])  # Первые 500 символов
