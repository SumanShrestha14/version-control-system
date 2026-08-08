#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class PackObjType {
  Commit = 1,
  Tree = 2,
  Blob = 3,
  Tag = 4,
  OfsDelta = 6,
  RefDelta = 7,
};

struct PackObject {
  PackObjType type;
  std::string data; // fully inflated bytes (object content, no git header)
  std::string base_ref_sha; // only set for RefDelta (raw 20 bytes)
  int64_t base_ofs_delta =
      0; // only set for OfsDelta (negative offset from this object's start)
  size_t offset_in_pack = 0; // start offset of this object's header, needed for
                             // ofs-delta resolution later
};

// Parses the whole pack buffer. Throws std::runtime_error on any malformed
// input. header_object_count is filled in from the pack header for a sanity
// check.
std::vector<PackObject> parse_packfile(const std::string &pack_data,
                                       uint32_t &header_object_count);

const char *pack_obj_type_name(PackObjType type);
struct ResolvedObject {
  PackObjType type; // always Commit/Tree/Blob/Tag -- never a delta type after
                    // resolution
  std::string data;
};

std::string apply_delta(const std::string &base, const std::string &delta);

// Resolves every object in `objects` -- including delta chains of arbitrary
// depth -- into final type+content pairs, in the same order as the input.
std::vector<ResolvedObject>
resolve_pack_objects(const std::vector<PackObject> &objects);
