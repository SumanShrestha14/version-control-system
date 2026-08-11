#include "compressor.h"
#include <stdexcept>
#include <vector>
#include <zlib.h>

std::string Compressor::compress(const std::string &data) {
  uLongf bound = compressBound(data.size());
  std::vector<unsigned char> buffer(bound);

  int result = ::compress(buffer.data(), &bound,
                          reinterpret_cast<const unsigned char *>(data.data()),
                          data.size());
  if (result != Z_OK) {
    throw std::runtime_error("Compressor::compress: zlib error " +
                             std::to_string(result));
  }
  return std::string(reinterpret_cast<char *>(buffer.data()), bound);
}

std::string Compressor::decompress(const std::string &compressedData) {
  uLongf decompressedSize = compressedData.size() * 4 + 64;
  std::vector<Bytef> buffer;
  int result;
  do {
    buffer.resize(decompressedSize);
    result = uncompress(buffer.data(), &decompressedSize,
                        reinterpret_cast<const Bytef *>(compressedData.data()),
                        compressedData.size());
    if (result == Z_BUF_ERROR) {
      decompressedSize *= 2;
    }
  } while (result == Z_BUF_ERROR);

  if (result != Z_OK) {
    throw std::runtime_error("Compressor::decompress: zlib error " +
                             std::to_string(result));
  }
  return std::string(reinterpret_cast<char *>(buffer.data()), decompressedSize);
}
