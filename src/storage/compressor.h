#pragma once
#include <string>

class Compressor {
public:
    static std::string compress(const std::string& data);
    static std::string decompress(const std::string& compressedData);

    Compressor() = delete;   // stateless utility -- never meant to be instantiated
};
