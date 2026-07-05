with open("src/scanner.c", "r", encoding="utf-8") as f:
    lines = f.readlines()

# 行号: 第141行(0-indexed: 140)到第234行(0-indexed: 233)
# 第141行 = } else if (compression == 8) {
# 第234行 = }
start = 140
end = 233

# 验证
assert lines[start].strip() == "} else if (compression == 8) {", f"Expected 'else if...' but got: {repr(lines[start])}"
assert lines[end].strip() == "}", f"Expected closing brace but got: {repr(lines[end])}"

print(f"Confirming: lines[{start}] = {repr(lines[start].rstrip())}")
print(f"Confirming: lines[{end}] = {repr(lines[end].rstrip())}")

new_block = """                    } else if (compression == 8) {
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

# 用新块替换从 start 到 end+1 的行
new_lines = lines[:start] + [new_block + "\n"] + lines[end+1:]

with open("src/scanner.c", "w", encoding="utf-8") as f:
    f.writelines(new_lines)

print(f"Replaced lines {start+1}-{end+1} ({end-start+1} lines) with new block")
print(f"New file: {len(new_lines)} lines (was {len(lines)} lines)")
