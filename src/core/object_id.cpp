#include "object_id.h"
#include <cstdio>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>
#include <stdexcept>

ObjectId ObjectId::fromHex(const std::string &hex) {
  if (hex.size() != 40) {
    throw std::runtime_error("ObjectId::fromHex: invalid length: " + hex);
  }
  for (char c : hex) {
    bool isHexDigit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    if (!isHexDigit) {
      throw std::runtime_error("ObjectId::fromHex: invalid hex char in " + hex);
    }
  }
  return ObjectId(hex);
}

ObjectId ObjectId::fromRaw(const std::string &raw20) {
  if (raw20.size() != 20) {
    throw std::runtime_error("ObjectId::fromRaw: expected 20 bytes, got " +
                             std::to_string(raw20.size()));
  }
  static const char *digits = "0123456789abcdef";
  std::string hex;
  hex.reserve(40);
  for (unsigned char c : raw20) {
    hex.push_back(digits[(c >> 4) & 0xF]);
    hex.push_back(digits[c & 0xF]);
  }
  return ObjectId(hex);
}

ObjectId ObjectId::hashOf(const std::string &storeBytes) {
  unsigned char digest[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char *>(storeBytes.data()),
       storeBytes.size(), digest);
  return fromRaw(
      std::string(reinterpret_cast<char *>(digest), SHA_DIGEST_LENGTH));
}

std::string ObjectId::raw() const {
  if (hex_.size() != 40) {
    throw std::runtime_error("ObjectId::raw: not initialized");
  }
  std::string raw;
  raw.reserve(20);
  for (size_t i = 0; i < 40; i += 2) {
    unsigned int byte;
    std::sscanf(hex_.substr(i, 2).c_str(), "%02x", &byte);
    raw.push_back(static_cast<char>(byte));
  }
  return raw;
}
