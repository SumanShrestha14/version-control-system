#include "tree.h"
#include <algorithm>
#include <stdexcept>

void Tree::sortEntries() {
  std::sort(entries_.begin(), entries_.end(),
            [](const TreeEntry &a, const TreeEntry &b) {
              return a.sortKey() < b.sortKey();
            });
}

std::string Tree::serialize() const {
  std::string content;
  for (const auto &e : entries_) {
    content += e.encode();
  }
  return content;
}

Tree Tree::parse(const std::string &storeBytes) {
  std::string body = stripHeader(storeBytes, "tree");

  std::vector<TreeEntry> entries;
  std::size_t pos = 0;
  while (pos < body.size()) {
    std::size_t spacePos = body.find(' ', pos);
    if (spacePos == std::string::npos) {
      throw std::runtime_error("Malformed tree entry: missing mode separator");
    }
    std::string mode = body.substr(pos, spacePos - pos);
    pos = spacePos + 1;

    std::size_t nullPos = body.find('\0', pos);
    if (nullPos == std::string::npos) {
      throw std::runtime_error("Malformed tree entry: missing name terminator");
    }
    std::string name = body.substr(pos, nullPos - pos);
    pos = nullPos + 1;

    if (pos + 20 > body.size()) {
      throw std::runtime_error("Malformed tree entry: truncated SHA-1");
    }
    ObjectId sha = ObjectId::fromRaw(body.substr(pos, 20));
    pos += 20;

    entries.emplace_back(mode, name, sha);
  }
  return Tree(entries);
}
