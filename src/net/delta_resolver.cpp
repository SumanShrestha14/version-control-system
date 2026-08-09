//simply delta file lai reolve garne 
#include "delta_resolver.h"
#include "../core/object_id.h"
#include <stdexcept>
#include <unordered_map>//standard header file that stores elements in key header file using hasj for fast access

namespace {
uint64_t readDeltaVarint(const std::string &data, size_t &pos) {
  uint64_t result = 0;
  int shift = 0;
  uint8_t byte;
  do {
    if (pos >= data.size())
      throw std::runtime_error("DeltaResolver: truncated varint");
    byte = static_cast<uint8_t>(data[pos++]);
    result |= static_cast<uint64_t>(byte & 0x7F) << shift;
    shift += 7;
  } while (byte & 0x80);
  return result;
}
} // namespace

std::string DeltaResolver::applyDelta(const std::string &base,
                                      const std::string &delta) {
  size_t pos = 0;
  uint64_t baseSize = readDeltaVarint(delta, pos);//read the delta size /object
  uint64_t targetSize = readDeltaVarint(delta, pos);

  if (baseSize != base.size()) {//checking about how much size it could go after reoslving
    throw std::runtime_error("DeltaResolver: base size mismatch (expected " +
                             std::to_string(baseSize) + ", got " +
                             std::to_string(base.size()) + ")");
  }

  std::string result;
  result.reserve(targetSize);

  while (pos < delta.size()) {//main delta loop
    uint8_t opcode = static_cast<uint8_t>(delta[pos++]);

    if (opcode & 0x80) {//if highest bit is set or not vanera herne
      uint32_t offset = 0, size = 0;
      size_t operandBytes = 0;
      for (int bit = 0; bit < 7; ++bit)
        if (opcode & (1 << bit))
          ++operandBytes;
      if (pos + operandBytes > delta.size())
        throw std::runtime_error("DeltaResolver: truncated copy instruction");

      if (opcode & 0x01)
        offset |= static_cast<uint8_t>(delta[pos++]);
      if (opcode & 0x02)
        offset |= static_cast<uint32_t>(static_cast<uint8_t>(delta[pos++]))
                  << 8;
      if (opcode & 0x04)
        offset |= static_cast<uint32_t>(static_cast<uint8_t>(delta[pos++]))//for checking offset multiple if
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
        size = 0x10000;

      if (static_cast<uint64_t>(offset) + size > base.size()) {
        throw std::runtime_error(
            "DeltaResolver: copy instruction reads past end of base object");
      }
      result.append(base, offset, size);
    } else if (opcode != 0) {//byte directly contain in data
      size_t len = opcode;
      if (pos + len > delta.size())
        throw std::runtime_error("DeltaResolver: truncated insert data");
      result.append(delta, pos, len);//actual copying
      pos += len;
    } else {
      throw std::runtime_error(
          "DeltaResolver: opcode byte 0 is reserved/invalid");
    }
  }

  if (result.size() != targetSize) {//expected vs actual
    throw std::runtime_error(
        "DeltaResolver: reconstructed size mismatch (expected " +
        std::to_string(targetSize) + ", got " + std::to_string(result.size()) +
        ")");
  }
  return result;
}

std::vector<ResolvedObject>
DeltaResolver::resolve(const std::vector<PackObject> &objects) {
  size_t n = objects.size();
  std::vector<ResolvedObject> resolved(n);
  std::vector<bool> done(n, false);

  std::unordered_map<size_t, size_t> offsetToIndex;
  for (size_t i = 0; i < n; ++i)
    offsetToIndex[objects[i].offsetInPack] = i;

  std::unordered_map<std::string, size_t>
      shaToIndex; // raw 20-byte sha -> index

  auto rawHashOf = [](PackObjType type, const std::string &data) {
    std::string store = std::string(packObjTypeName(type)) + " " +
                        std::to_string(data.size()) + '\0' + data;
    return ObjectId::hashOf(store).raw();
  };

  size_t resolvedCount = 0;
  size_t prevCount = static_cast<size_t>(-1);

  while (resolvedCount != prevCount && resolvedCount < n) {
    prevCount = resolvedCount;

    for (size_t i = 0; i < n; ++i) {
      if (done[i])
        continue;
      const PackObject &obj = objects[i];

      if (obj.type != PackObjType::OfsDelta &&
          obj.type != PackObjType::RefDelta) {//yes this is not delta finished resolving
        resolved[i] = ResolvedObject{obj.type, obj.data};
        done[i] = true;
        shaToIndex[rawHashOf(obj.type, obj.data)] = i;
        resolvedCount++;
        continue;
      }

      size_t baseIdx;
      if (obj.type == PackObjType::OfsDelta) {
        auto it = offsetToIndex.find(static_cast<size_t>(obj.baseOfsDelta));
        if (it == offsetToIndex.end()) {
          throw std::runtime_error(
              "DeltaResolver: ofs-delta base offset not found in pack");
        }
        baseIdx = it->second;
      } else {
        auto it = shaToIndex.find(obj.baseRefSha);
        if (it == shaToIndex.end())
          continue; // base not resolved yet -- retry next pass
        baseIdx = it->second;
      }

      if (!done[baseIdx])
        continue;

      const ResolvedObject &base = resolved[baseIdx];//applt delta after base finding
      resolved[i].type = base.type;
      resolved[i].data = applyDelta(base.data, obj.data);
      done[i] = true;
      shaToIndex[rawHashOf(resolved[i].type, resolved[i].data)] = i;
      resolvedCount++;
    }
  }

  if (resolvedCount < n) {
    throw std::runtime_error(
        "DeltaResolver: " + std::to_string(n - resolvedCount) +
        " delta object(s) could not be resolved (cycle, or base missing from "
        "pack)");
  }
  return resolved;

}
//It first identifies the base object using either its pack offset or SHA-1, 
//then applies the delta's copy and insert instructions to the base data.
// It repeats this process to 
//resolve delta chains and finally produces complete Blob, Tree, or Commit objects
