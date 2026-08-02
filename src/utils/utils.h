#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <fstream>
#include <iostream>
#include <zlib.h>
#include <stdexcept>
#include <vector>

struct TreeEntry {
    std::string mode;   // e.g. "100644", "40000" (no leading zero, as stored)
    std::string name;   // filename or directory name
    std::string sha1;   // 40-char lowercase hex string
};
// struct WriteTreeEntry {
//   std::string mode;     // "100644", "100755", "40000"
//   std::string name;
//   std::string raw_sha;  // 20 raw bytes, NOT hex
// };

std::string decompress(const std::string& compressedData);
std::string read_file(const std::string& file_path);
std::string sha1_hex(const std::string& str);
std::string compress(const std::string& data);
void write_object_file(const std::string& hash, const std::string& compressed);
std::string mode_to_type(const std::string &mode);
std::vector<TreeEntry> parse_tree_object(const std::string &decompressed);
std::string pad_mode(const std::string &mode);
std::string read_object(const std::string &sha1);
std::string sha1_raw(const std::string &data);
std::string hex_to_raw(const std::string &hex);
std::string current_git_timestamp();
#endif
