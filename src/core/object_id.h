#pragma once
#include <string>

// Represents a Git object's SHA-1 identity. Always stores hex internally so
// there's never ambiguity at a call site about whether a string is hex or
// raw bytes -- that ambiguity was the root cause of the "double-hashing"
// bug class (hashing an already-hashed value, or writing raw bytes where
// hex was expected).

class ObjectId {
public:
    ObjectId() = default;

    static ObjectId fromHex(const std::string& hex);
    static ObjectId fromRaw(const std::string& raw20);      // 20 raw bytes
    static ObjectId hashOf(const std::string& storeBytes);   // header+body -> SHA1

    const std::string& hex() const { return hex_; }
    std::string raw() const;          // converts hex_ -> 20 raw bytes on demand

    bool empty() const { return hex_.empty(); }
    bool operator==(const ObjectId& other) const { return hex_ == other.hex_; }
    bool operator!=(const ObjectId& other) const { return !(*this == other); }

private:
    explicit ObjectId(std::string hex) : hex_(std::move(hex)) {}
    std::string hex_;   // 40 lowercase hex chars, or empty
};
