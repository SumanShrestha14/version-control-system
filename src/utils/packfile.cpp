#include "packfile.h"
#include "utils.h" // for sha1_raw, used to identify ref-delta bases by hash
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <zlib.h>

namespace {

uint32_t read_u32be(const std::string &data, size_t pos) {
  if (pos + 4 > data.size())
    throw std::runtime_error("packfile: truncated header");
  return (static_cast<uint8_t>(data[pos]) << 24) |
         (static_cast<uint8_t>(data[pos + 1]) << 16) |
         (static_cast<uint8_t>(data[pos + 2]) << 8) |
         (static_cast<uint8_t>(data[pos + 3]));
}

// Decodes the type+size varint header at `pos`. Advances `pos` past it.
// Returns (type_bits, size).
std::pair<int, uint64_t> read_type_and_size(const std::string &data,
                                            size_t &pos) {
  if (pos >= data.size())
    throw std::runtime_error("packfile: truncated object header");

  uint8_t first = static_cast<uint8_t>(data[pos++]);
  int type_bits = (first >> 4) & 0x7;
  uint64_t size = first & 0x0F;
  int shift = 4;

  bool more = (first & 0x80) != 0;
  while (more) {
    if (pos >= data.size())
      throw std::runtime_error("packfile: truncated size varint");
    uint8_t byte = static_cast<uint8_t>(data[pos++]);
    size |= static_cast<uint64_t>(byte & 0x7F) << shift;
    shift += 7;
    more = (byte & 0x80) != 0;
  }

  return {type_bits, size};
}

// Reads the negative offset used by ofs-delta objects. Different bit-packing
// than the size varint above: base-128, MSB=continuation, but each
// continuation byte's value is offset by +1 (the "sneaky" encoding), because
// the naive encoding would allow multiple representations of the same value.
int64_t read_ofs_delta_offset(const std::string &data, size_t &pos) {
  if (pos >= data.size())
    throw std::runtime_error("packfile: truncated ofs-delta offset");

  uint8_t byte = static_cast<uint8_t>(data[pos++]);
  int64_t result = byte & 0x7F;

  while (byte & 0x80) {
    if (pos >= data.size())
      throw std::runtime_error("packfile: truncated ofs-delta offset");
    byte = static_cast<uint8_t>(data[pos++]);
    result += 1; // the "+1 per continuation byte" quirk
    result = (result << 7) | (byte & 0x7F);
  }

  return result;
}

// Inflates a raw zlib stream starting at `pos` without knowing its length in
// advance. Stops the moment inflate() reports Z_STREAM_END, and advances
// `pos` by exactly the number of compressed bytes consumed -- this is what
// lets us find the start of the *next* object in the pack.
std::string inflate_stream(const std::string &data, size_t &pos) {
  z_stream strm{};
  if (inflateInit(&strm) != Z_OK) {
    throw std::runtime_error("packfile: inflateInit failed");
  }

  std::string output;
  std::vector<char> out_buf(64 * 1024);

  strm.next_in =
      reinterpret_cast<Bytef *>(const_cast<char *>(data.data() + pos));
  strm.avail_in = static_cast<uInt>(data.size() - pos);

  int ret;
  do {
    strm.next_out = reinterpret_cast<Bytef *>(out_buf.data());
    strm.avail_out = static_cast<uInt>(out_buf.size());

    ret = inflate(&strm, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
      inflateEnd(&strm);
      throw std::runtime_error(std::string("packfile: inflate() failed: ") +
                               (strm.msg ? strm.msg : "unknown"));
    }

    size_t produced = out_buf.size() - strm.avail_out;
    output.append(out_buf.data(), produced);
  } while (ret != Z_STREAM_END);

  size_t consumed = strm.total_in;
  inflateEnd(&strm);

  pos += consumed;
  return output;
}

} // namespace

const char *pack_obj_type_name(PackObjType type) {
  switch (type) {
  case PackObjType::Commit:
    return "commit";
  case PackObjType::Tree:
    return "tree";
  case PackObjType::Blob:
    return "blob";
  case PackObjType::Tag:
    return "tag";
  case PackObjType::OfsDelta:
    return "ofs-delta";
  case PackObjType::RefDelta:
    return "ref-delta";
  }
  return "unknown";
}

std::vector<PackObject> parse_packfile(const std::string &pack_data,
                                       uint32_t &header_object_count) {
  if (pack_data.size() < 12 || pack_data.substr(0, 4) != "PACK") {
    throw std::runtime_error("packfile: missing PACK signature");
  }

  uint32_t version = read_u32be(pack_data, 4);
  if (version != 2 && version != 3) {
    throw std::runtime_error("packfile: unsupported version " +
                             std::to_string(version));
  }

  header_object_count = read_u32be(pack_data, 8);

  std::vector<PackObject> objects;
  objects.reserve(header_object_count);

  size_t pos = 12;
  for (uint32_t i = 0; i < header_object_count; ++i) {
    size_t object_start = pos;
    auto [type_bits, size] = read_type_and_size(pack_data, pos);
    (void)size; // size is a hint for buffer pre-allocation in real git; we
                // don't need it since inflate tells us actual length

    PackObject obj;
    obj.offset_in_pack = object_start;

    switch (type_bits) {
    case 1:
      obj.type = PackObjType::Commit;
      break;
    case 2:
      obj.type = PackObjType::Tree;
      break;
    case 3:
      obj.type = PackObjType::Blob;
      break;
    case 4:
      obj.type = PackObjType::Tag;
      break;
    case 6:
      obj.type = PackObjType::OfsDelta;
      break;
    case 7:
      obj.type = PackObjType::RefDelta;
      break;
    default:
      throw std::runtime_error("packfile: unknown object type " +
                               std::to_string(type_bits));
    }

    if (obj.type == PackObjType::OfsDelta) {
      int64_t back_offset = read_ofs_delta_offset(pack_data, pos);
      obj.base_ofs_delta = static_cast<int64_t>(object_start) - back_offset;
    } else if (obj.type == PackObjType::RefDelta) {
      if (pos + 20 > pack_data.size())
        throw std::runtime_error("packfile: truncated ref-delta base sha");
      obj.base_ref_sha = pack_data.substr(pos, 20);
      pos += 20;
    }

    obj.data = inflate_stream(
        pack_data, pos); // for deltas, this is the delta instruction stream,
                         // not final content -- resolved in M4/M5

    objects.push_back(std::move(obj));
  }

  return objects;
}

namespace {

uint64_t read_delta_varint(const std::string &data, size_t &pos) {
  uint64_t result = 0;
  int shift = 0;
  uint8_t byte;
  do {
    if (pos >= data.size())
      throw std::runtime_error("delta: truncated varint");
    byte = static_cast<uint8_t>(data[pos++]);
    result |= static_cast<uint64_t>(byte & 0x7F) << shift;
    shift += 7;
  } while (byte & 0x80);
  return result;
}

} // namespace

std::string apply_delta(const std::string &base, const std::string &delta) {
  size_t pos = 0;
  uint64_t base_size = read_delta_varint(delta, pos);
  uint64_t target_size = read_delta_varint(delta, pos);

  if (base_size != base.size()) {
    throw std::runtime_error("delta: base size mismatch (expected " +
                             std::to_string(base_size) + ", got " +
                             std::to_string(base.size()) + ")");
  }

  std::string result;
  result.reserve(target_size);

  while (pos < delta.size()) {
    uint8_t opcode = static_cast<uint8_t>(delta[pos++]);

    if (opcode & 0x80) {
      // COPY: bits 0-3 select present offset bytes, bits 4-6 select present
      // size bytes
      uint32_t offset = 0, size = 0;
      if (opcode & 0x01)
        offset |= static_cast<uint8_t>(delta[pos++]);
      if (opcode & 0x02)
        offset |= static_cast<uint32_t>(static_cast<uint8_t>(delta[pos++]))
                  << 8;
      if (opcode & 0x04)
        offset |= static_cast<uint32_t>(static_cast<uint8_t>(delta[pos++]))
                  << 16;
      if (opcode & 0x08)
        offset |= static_cast<uint32_t>(static_cast<uint8_t>(delta[pos++]))
                  << 24;
      if (opcode & 0x10)
        size |= static_cast<uint8_t>(delta[pos++]);
      if (opcode & 0x20)
        size |= static_cast<uint32_t>(static_cast<uint8_t>(delta[pos++])) << 8;
      if (opcode & 0x40)
        size |= static_cast<uint32_t>(static_cast<uint8_t>(delta[pos++])) << 16;
      if (size == 0)
        size = 0x10000; // 3 size bytes can't encode 65536 directly

      if (static_cast<uint64_t>(offset) + size > base.size()) {
        throw std::runtime_error(
            "delta: copy instruction reads past end of base object");
      }
      result.append(base, offset, size);
    } else if (opcode != 0) {
      // INSERT: opcode itself is the literal byte count (1-127)
      size_t len = opcode;
      if (pos + len > delta.size())
        throw std::runtime_error("delta: truncated insert data");
      result.append(delta, pos, len);
      pos += len;
    } else {
      throw std::runtime_error("delta: opcode byte 0 is reserved/invalid");
    }
  }

  if (result.size() != target_size) {
    throw std::runtime_error("delta: reconstructed size mismatch (expected " +
                             std::to_string(target_size) + ", got " +
                             std::to_string(result.size()) + ")");
  }
  return result;
}

std::vector<ResolvedObject>
resolve_pack_objects(const std::vector<PackObject> &objects) {
  size_t n = objects.size();
  std::vector<ResolvedObject> resolved(n);
  std::vector<bool> done(n, false);

  std::unordered_map<size_t, size_t> offset_to_index;
  for (size_t i = 0; i < n; ++i)
    offset_to_index[objects[i].offset_in_pack] = i;

  std::unordered_map<std::string, size_t>
      sha_to_index; // raw 20-byte git sha -> index, populated as objects
                    // resolve

  auto git_raw_hash_of = [](PackObjType type, const std::string &data) {
    std::string store = std::string(pack_obj_type_name(type)) + " " +
                        std::to_string(data.size()) + '\0' + data;
    return sha1_raw(store);
  };

  size_t resolved_count = 0;
  size_t prev_count = static_cast<size_t>(-1);

  // Iterative fixed-point: keep sweeping until a full pass makes no
  // progress. Handles arbitrary delta chain depth and either delta
  // addressing scheme without needing to know pack ordering in advance.
  while (resolved_count != prev_count && resolved_count < n) {
    prev_count = resolved_count;

    for (size_t i = 0; i < n; ++i) {
      if (done[i])
        continue;
      const PackObject &obj = objects[i];

      if (obj.type != PackObjType::OfsDelta &&
          obj.type != PackObjType::RefDelta) {
        resolved[i] = ResolvedObject{obj.type, obj.data};
        done[i] = true;
        sha_to_index[git_raw_hash_of(obj.type, obj.data)] = i;
        resolved_count++;
        continue;
      }

      size_t base_idx;
      if (obj.type == PackObjType::OfsDelta) {
        auto it = offset_to_index.find(static_cast<size_t>(obj.base_ofs_delta));
        if (it == offset_to_index.end()) {
          throw std::runtime_error(
              "packfile: ofs-delta base offset not found in pack");
        }
        base_idx = it->second;
      } else { // RefDelta
        auto it = sha_to_index.find(obj.base_ref_sha);
        if (it == sha_to_index.end())
          continue; // base's hash not known yet -- retry next pass
        base_idx = it->second;
      }

      if (!done[base_idx])
        continue; // base identified but not yet resolved -- retry next pass

      const ResolvedObject &base = resolved[base_idx];
      resolved[i].type = base.type;
      resolved[i].data = apply_delta(base.data, obj.data);
      done[i] = true;
      sha_to_index[git_raw_hash_of(resolved[i].type, resolved[i].data)] = i;
      resolved_count++;
    }
  }

  if (resolved_count < n) {
    throw std::runtime_error("packfile: " + std::to_string(n - resolved_count) +
                             " delta object(s) could not be resolved (cycle, "
                             "or base missing from pack)");
  }

  return resolved;
}
