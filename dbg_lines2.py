with open("src/scanner.c", "r", encoding="utf-8") as f:
    lines = f.readlines()
for i in range(128, 195):
    print(f"{i+1}: {repr(lines[i])}")
