import re
with open("src/scanner.c", "r", encoding="utf-8") as f:
    content = f.read()

# 找到 } else if (compression == 8) { 这行
marker = "} else if (compression == 8) {"
pos = content.find(marker)
if pos < 0:
    print("ERROR: marker not found!")
    exit(1)

# 从这往后找到第二个连续的 }（即外层 if 结构结束），然后是 break;
# 原始的 PowerShell 块结构是:
# } else if (compression == 8) {
#     // ... 大量代码
#     system(rmdir_cmd);
#     }
# }  ← 这个 } 是 else if 的结束

# 先找到 system(rmdir_cmd); 的下一行
system_pos = content.rfind("system(rmdir_cmd);", pos, pos + 4000)
if system_pos < 0:
    print("ERROR: system(rmdir_cmd) not found!")
    exit(1)

# system(rmdir_cmd); 之后一行是 }
line_end = content.find("\n", system_pos)
if line_end < 0:
    print("ERROR: line end not found after system")
    exit(1)

# 再下一行应该是 }
next_line = content.find("\n", line_end + 1)
if next_line < 0:
    print("ERROR: next line not found")
    exit(1)

# 获取 } 所在的行
brace_line = content[line_end+1:next_line]
print(f"Line after system(rmdir_cmd): {repr(brace_line)}")

# 获取下一行（应该是 break; 那一层的 }
next_line2 = content.find("\n", next_line + 1)
after_brace = content[next_line+1:next_line2]
print(f"Line after that: {repr(after_brace)}")

# 这里的 old_end 应该是到第二个 } 之后
old_end = next_line2

old_text = content[pos:old_end]
print(f"=== OLD TEXT ===")
print(old_text[:500])
print(f"... total {len(old_text)} chars")

# 新的代码
new_code = """                    } else if (compression == 8) {
                        // DEFLATE 压缩 — 使用 zlib inflate 直接解压（比 PowerShell 快百倍）
                        unsigned int comp_size = lf_compressed > 0 ? lf_compressed : compressed_size;
                        unsigned int uncomp_size = uncompressed_size;
                        if (comp_size > 0 && uncomp_size > 0 &&
                            data_offset + comp_size <= fsize) {
                            z_stream strm;
                            memset(&strm, 0, sizeof(strm));
                            strm.next_in = (unsigned char *)(data + data_offset);
                            strm.avail_in = comp_size;

                            *output = malloc(uncomp_size + 1);
                            if (*output) {
                                strm.next_out = (unsigned char *)(*output);
                                strm.avail_out = uncomp_size;

                                int ret = inflateInit2(&strm, -MAX_WBITS);
                                if (ret == Z_OK) {
                                    ret = inflate(&strm, Z_FINISH);
                                    if (ret == Z_STREAM_END || ret == Z_OK) {
                                        unsigned int written = uncomp_size - strm.avail_out;
                                        (*output)[written] = '\\0';
                                        *output_len = written;
                                        result = 0;
                                    }
                                    inflateEnd(&strm);
                                }
                                if (result != 0) {
                                    free(*output);
                                    *output = NULL;
                                }
                            }
                        }
                    }"""

new_content = content[:pos] + new_code + content[old_end:]

with open("src/scanner.c", "w", encoding="utf-8") as f:
    f.write(new_content)

print(f"\n=== DONE ===")
print(f"Replaced {len(old_text)} chars with {len(new_code)} chars")
print(f"New file size: {len(new_content)} bytes (was {len(content)} bytes)")
