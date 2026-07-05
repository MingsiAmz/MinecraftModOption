with open("src/scanner.c", "r", encoding="utf-8") as f:
    content = f.read()
# 从 } else if (compression == 8) { 开始找
pos = content.find('} else if (compression == 8) {')
if pos >= 0:
    # 找之前的行首
    start = content.rfind('\n', 0, pos) + 1
    # 找结束 - 到下一个 break; 后面的 }
    block = content[start:start+2500]
    # 显示具体的 repr
    print(repr(block[:2000]))
else:
    print("NOT FOUND")
