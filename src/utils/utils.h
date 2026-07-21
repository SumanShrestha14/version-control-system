#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <fstream>
#include <iostream>
#include <string>
#include <zlib.h>
#include <stdexcept>

std::string decompress(const std::string& compressedData);
std::string read_file(const std::string& file_path);
std::string sha1_hex(const std::string& str);
std::string compress(const std::string& data);
void write_object_file(const std::string& hash, const std::string& compressed);

#endif
