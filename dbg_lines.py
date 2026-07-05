with open("src/scanner.c", "r", encoding="utf-8") as f:
    lines = f.readlines()
for i in range(128, 200):
    r = repr(lines[i])
    if i >= 128 and i <= 132:
        print(f"{i+1}: {r}")
