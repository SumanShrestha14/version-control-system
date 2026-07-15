#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <fstream>
#include <iostream>
#include <string>
#include <zlib.h>
#include <stdexcept>

std::string decompress(const std::string& compressedData);

#endif
