#pragma once
#include "../core/object_id.h"
#include <cstdint>
#include <string>
#include <vector>

enum class PackObjType { Commit, Tree, Blob, Tag, OfsDelta, RefDelta };
const char* packObjTypeName(PackObjType type);

struct PackObject {
    PackObjType type;
    std::string data;              // for deltas: the delta instruction stream, not final content
    size_t offsetInPack = 0;
    int64_t baseOfsDelta = -1;     // valid only if type == OfsDelta
    std::string baseRefSha;        // 20 raw bytes, valid only if type == RefDelta
};

class PackfileParser {
public:
    explicit PackfileParser(std::string packData) : data_(std::move(packData)) {}

    // Parses the full pack. Fills headerObjectCount with the count declared
    // in the pack header (may differ from returned size only on malformed input,
    // which parse() rejects before returning).
    std::vector<PackObject> parse(uint32_t& headerObjectCount);

private:
    std::string data_;
    size_t pos_ = 0;

    uint32_t readU32be(size_t at) const;
    std::pair<int, uint64_t> readTypeAndSize();     // advances pos_
    int64_t readOfsDeltaOffset();                    // advances pos_
    std::string inflateNextStream();                  // advances pos_ by compressed bytes consumed
};
