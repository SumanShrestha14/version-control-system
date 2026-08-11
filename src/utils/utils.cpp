#include "utils.h"
#include <ctime>
#include <filesystem>
#include <openssl/sha.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>

using namespace std;

string decompress(const string &compressed) {
  uLongf decompressedSize = compressed.size() * 4 + 64;
  vector<Bytef> buffer;
  int result;
  do {
    buffer.resize(decompressedSize);
    result = uncompress(buffer.data(), &decompressedSize,
                        reinterpret_cast<const Bytef *>(compressed.data()),
                        compressed.size());
    if (result == Z_BUF_ERROR) {
      decompressedSize = decompressedSize * 2;
    }
  } while (result == Z_BUF_ERROR);
  if (result != Z_OK) {
    throw runtime_error("zlib decompression error: " + to_string(result));
  }

  return string(reinterpret_cast<char *>(buffer.data()), decompressedSize);
}

string read_file(const string &file_path) {
  ifstream file(file_path, ios::binary);
  if (!file.is_open()) {
    throw runtime_error("failed to open file: " + file_path);
  }
  return string(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
}

string sha1_hex(const string &data) {
  unsigned char digest[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char *>(data.data()), data.size(),
       digest);

  ostringstream oss;
  for (unsigned char byte : digest) {
    oss << hex << setw(2) << setfill('0') << static_cast<int>(byte);
  }
  return oss.str();
}

string compress(const string &data) {
  uLongf bound = compressBound(data.size());
  vector<unsigned char> buffer(bound);

  int result = compress(buffer.data(), &bound,
                        reinterpret_cast<const unsigned char *>(data.data()),
                        data.size());
  if (result != Z_OK) {
    throw runtime_error("zlib compression failed: " + to_string(result));
  }
  return string(reinterpret_cast<char *>(buffer.data()), bound);
}

void write_object_file(const string &hash, const string &compressed) {
  string dir = ".synk/objects/" + hash.substr(0, 2);
  string path = dir + "/" + hash.substr(2);

  filesystem::create_directories(dir);

  ofstream out(path, ios::binary);
  if (!out) {
    throw runtime_error("Failed to write object file: " + path);
  }
  out.write(compressed.data(), static_cast<streamsize>(compressed.size()));
}

string raw_sha_to_hex(const std::string &raw) {
  static const char *hex_digits = "0123456789abcdef";
  std::string out;
  out.reserve(40);
  for (unsigned char c : raw) {
    out.push_back(hex_digits[(c >> 4) & 0xF]);
    out.push_back(hex_digits[c & 0xF]);
  }
  return out;
}

vector<TreeEntry> parse_tree_object(const std::string &decompressed) {
  std::vector<TreeEntry> entries;

  // --- Skip the header: "tree <size>\0" ---
  std::size_t pos = decompressed.find('\0');
  if (pos == std::string::npos) {
    throw std::runtime_error(
        "Malformed tree object: missing header terminator");
  }
  pos += 1; // move past the header's null byte

  // --- Walk entries: "<mode> <name>\0<20 raw bytes>" repeated ---
  while (pos < decompressed.size()) {
    // 1. Read mode up to the space
    std::size_t space_pos = decompressed.find(' ', pos);
    if (space_pos == std::string::npos) {
      throw std::runtime_error("Malformed tree entry: missing mode separator");
    }
    std::string mode = decompressed.substr(pos, space_pos - pos);
    pos = space_pos + 1;

    // 2. Read name up to the next null byte
    std::size_t null_pos = decompressed.find('\0', pos);
    if (null_pos == std::string::npos) {
      throw std::runtime_error("Malformed tree entry: missing name terminator");
    }
    std::string name = decompressed.substr(pos, null_pos - pos);
    pos = null_pos + 1;

    // 3. Read exactly 20 raw bytes for the SHA-1 (NOT hex, NOT null-terminated)
    if (pos + 20 > decompressed.size()) {
      throw std::runtime_error("Malformed tree entry: truncated SHA-1");
    }
    std::string raw_sha = decompressed.substr(pos, 20);
    pos += 20;

    entries.push_back(TreeEntry{mode, name, raw_sha_to_hex(raw_sha)});
  }

  return entries;
}

string mode_to_type(const std::string &mode) {
  if (mode == "40000")
    return "tree";
  if (mode == "160000")
    return "commit"; // submodule (gitlink)
  return "blob";     // covers 100644, 100755, 120000 (symlink)
}

string pad_mode(const std::string &mode) {
  std::string padded = mode;
  while (padded.size() < 6)
    padded = "0" + padded;
  return padded;
}

std::string read_object(const std::string &sha1) {
  if (sha1.length() != 40) {
    throw std::runtime_error("Invalid SHA-1 hash: " + sha1);
  }

  std::string path = ".synk/objects/" + sha1.substr(0, 2) + "/" + sha1.substr(2);

  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open object file: " + path);
  }

  std::string compressed((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

  return decompress(compressed);
}

string sha1_raw(const string &data) {
  unsigned char digest[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char *>(data.data()), data.size(),
       digest);
  return string(reinterpret_cast<char *>(digest), SHA_DIGEST_LENGTH);
}

string hex_to_raw(const string &hex) {
  if (hex.length() != 40) {
    throw runtime_error("Invalid hex SHA-1 length: " + hex);
  }
  string raw;
  raw.reserve(20);
  for (size_t i = 0; i < 40; i += 2) {
    unsigned int byte;
    int matched = sscanf(hex.substr(i, 2).c_str(), "%02x", &byte);
    if (matched != 1) {
      throw runtime_error("Invalid hex SHA-1 characters: " + hex);
    }
    raw.push_back(static_cast<char>(byte));
  }
  return raw;
}
std::string current_synk_timestamp() {
  std::time_t now = std::time(nullptr);
  std::tm local_tm{};
  std::tm utc_tm{};
#ifdef _WIN32
  localtime_s(&local_tm, &now);
  gmtime_s(&utc_tm, &now);
#else
  localtime_r(&now, &local_tm);
  gmtime_r(&now, &utc_tm);
#endif

// Compute local offset from UTC in minutes, so the timezone field is
// actually correct for the machine running this, not just a hardcoded
// "+0000". mktime() on both broken-down times gives us the epoch-second
// difference directly.

  std::time_t local_as_time_t = std::mktime(&local_tm);
  std::time_t utc_as_time_t = std::mktime(&utc_tm);
  long offset_seconds = static_cast<long>(local_as_time_t - utc_as_time_t);

  char sign = offset_seconds >= 0 ? '+' : '-';
  long abs_offset_minutes = std::abs(offset_seconds) / 60;
  long hours = abs_offset_minutes / 60;
  long minutes = abs_offset_minutes % 60;

  char buf[8];
  std::snprintf(buf, sizeof(buf), "%c%02ld%02ld", sign, hours, minutes);

  return std::to_string(static_cast<long long>(now)) + " " + buf;
}
