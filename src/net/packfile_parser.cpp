#include "packfile_parser.h"
#include <zlib.h>
#include <stdexcept>

const char* packObjTypeName(PackObjType type) {
    switch (type) {
        case PackObjType::Commit:   return "commit";
        case PackObjType::Tree:     return "tree";
        case PackObjType::Blob:     return "blob";
        case PackObjType::Tag:      return "tag";
        case PackObjType::OfsDelta: return "ofs-delta";
        case PackObjType::RefDelta: return "ref-delta";
    }
    return "unknown";
}

uint32_t PackfileParser::readU32be(size_t at) const {
    if (at + 4 > data_.size()) throw std::runtime_error("PackfileParser: truncated header");
    return (static_cast<uint8_t>(data_[at]) << 24) |
           (static_cast<uint8_t>(data_[at + 1]) << 16) |
           (static_cast<uint8_t>(data_[at + 2]) << 8) |
           (static_cast<uint8_t>(data_[at + 3]));
}

std::pair<int, uint64_t> PackfileParser::readTypeAndSize() {
    if (pos_ >= data_.size()) throw std::runtime_error("PackfileParser: truncated object header");

    uint8_t first = static_cast<uint8_t>(data_[pos_++]);
    int typeBits = (first >> 4) & 0x7;
    uint64_t size = first & 0x0F;
    int shift = 4;

    bool more = (first & 0x80) != 0;
    while (more) {
        if (pos_ >= data_.size()) throw std::runtime_error("PackfileParser: truncated size varint");
        uint8_t byte = static_cast<uint8_t>(data_[pos_++]);
        size |= static_cast<uint64_t>(byte & 0x7F) << shift;
        shift += 7;
        more = (byte & 0x80) != 0;
    }
    return {typeBits, size};
}

int64_t PackfileParser::readOfsDeltaOffset() {
    if (pos_ >= data_.size()) throw std::runtime_error("PackfileParser: truncated ofs-delta offset");

    uint8_t byte = static_cast<uint8_t>(data_[pos_++]);
    int64_t result = byte & 0x7F;

    while (byte & 0x80) {
        if (pos_ >= data_.size()) throw std::runtime_error("PackfileParser: truncated ofs-delta offset");
        byte = static_cast<uint8_t>(data_[pos_++]);
        result += 1;   // "+1 per continuation byte" quirk -- prevents multiple encodings of the same value
        result = (result << 7) | (byte & 0x7F);
    }
    return result;
}

std::string PackfileParser::inflateNextStream() {
    z_stream strm{};
    if (inflateInit(&strm) != Z_OK) {
        throw std::runtime_error("PackfileParser: inflateInit failed");
    }

    std::string output;
    std::vector<char> outBuf(64 * 1024);

    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data_.data() + pos_));
    strm.avail_in = static_cast<uInt>(data_.size() - pos_);

    int ret;
    do {
        strm.next_out = reinterpret_cast<Bytef*>(outBuf.data());
        strm.avail_out = static_cast<uInt>(outBuf.size());

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            throw std::runtime_error(std::string("PackfileParser: inflate() failed: ") +
                                      (strm.msg ? strm.msg : "unknown"));
        }
        output.append(outBuf.data(), outBuf.size() - strm.avail_out);
    } while (ret != Z_STREAM_END);

    size_t consumed = strm.total_in;
    inflateEnd(&strm);
    pos_ += consumed;
    return output;
}

std::vector<PackObject> PackfileParser::parse(uint32_t& headerObjectCount) {
    if (data_.size() < 12 || data_.substr(0, 4) != "PACK") {
        throw std::runtime_error("PackfileParser: missing PACK signature");
    }
    uint32_t version = readU32be(4);
    if (version != 2 && version != 3) {
        throw std::runtime_error("PackfileParser: unsupported version " + std::to_string(version));
    }
    headerObjectCount = readU32be(8);
    if (headerObjectCount > data_.size() - 12) {
        throw std::runtime_error("PackfileParser: declared object count " +
                                  std::to_string(headerObjectCount) + " exceeds pack size");
    }

    std::vector<PackObject> objects;
    objects.reserve(headerObjectCount);
    pos_ = 12;

    for (uint32_t i = 0; i < headerObjectCount; ++i) {
        size_t objectStart = pos_;
        auto [typeBits, size] = readTypeAndSize();
        (void)size;   // hint only; inflate tells us the real length

        PackObject obj;
        obj.offsetInPack = objectStart;

        switch (typeBits) {
            case 1: obj.type = PackObjType::Commit; break;
            case 2: obj.type = PackObjType::Tree; break;
            case 3: obj.type = PackObjType::Blob; break;
            case 4: obj.type = PackObjType::Tag; break;
            case 6: obj.type = PackObjType::OfsDelta; break;
            case 7: obj.type = PackObjType::RefDelta; break;
            default: throw std::runtime_error("PackfileParser: unknown object type " + std::to_string(typeBits));
        }

        if (obj.type == PackObjType::OfsDelta) {
            int64_t backOffset = readOfsDeltaOffset();
            obj.baseOfsDelta = static_cast<int64_t>(objectStart) - backOffset;
        } else if (obj.type == PackObjType::RefDelta) {
            if (pos_ + 20 > data_.size()) throw std::runtime_error("PackfileParser: truncated ref-delta base sha");
            obj.baseRefSha = data_.substr(pos_, 20);
            pos_ += 20;
        }

        obj.data = inflateNextStream();
        objects.push_back(std::move(obj));
    }
    return objects;
}
