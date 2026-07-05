with open("src/scanner.c", "r", encoding="utf-8") as f:
    content = f.read()

# 找到从 } else if (compression == 8) { 到下一个紧接的 }
old_start = content.find('} else if (compression == 8) {')
# 找到这个闭括号结束的位置 - 找 } 后面紧跟一个新行 然后 break;
# 先找到两个 } 结束的位置
search_from = old_start
depth = 0
brace_start = -1
for i in range(old_start, len(content)):
    if content[i] == '{':
        depth += 1
        if brace_start < 0:
            brace_start = i
    elif content[i] == '}':
        depth -= 1
        if depth == 0 and brace_start > 0:
            # 这是完整的 if 块结束
            # 找之后的 break; 和 }
            rest = content[i:]
            end_brace = rest.find('}')
            if end_brace >= 0:
                old_end = i + end_brace + 1
                break
    if depth < 0:
        old_end = i + 1
        break

old_text = content[old_start:old_end]
print("=== OLD TEXT (first 200 chars) ===")
print(repr(old_text[:200]))
print(f"... length: {len(old_text)}")

new_text = """                    } else if (compression == 8) {
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

new_content = content[:old_start] + new_text + content[old_end:]

with open("src/scanner.c", "w", encoding="utf-8") as f:
    f.write(new_content)

print("=== Done ===")
print(f"Replaced {len(old_text)} bytes with {len(new_text)} bytes")
