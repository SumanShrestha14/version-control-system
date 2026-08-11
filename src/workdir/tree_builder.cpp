#include "tree_builder.h"
#include "../core/blob.h"
#include "../core/tree.h"
#include <iostream>
#include <fstream>

namespace {
bool isExecutable(const std::filesystem::path &path) {
  auto perms = std::filesystem::status(path).permissions();
  return (perms & std::filesystem::perms::owner_exec) !=
         std::filesystem::perms::none;
}

std::string readFileBytes(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("TreeBuilder: failed to open file: " +
                             path.string());
  }
  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}
} // namespace

std::optional<ObjectId>
TreeBuilder::buildSubtree(const std::filesystem::path &dir) {
  std::vector<TreeEntry> entries;

  for (const auto &dirent : std::filesystem::directory_iterator(dir)) {
    std::string name = dirent.path().filename().string();
    if (name == ".synk")
      continue;

    // Symlinks must be checked before is_directory()/is_regular_file(),
    // since those follow symlinks by default (status() vs symlink_status()).
    std::error_code ec;
    auto symStat = std::filesystem::symlink_status(dirent.path(), ec);
    if (ec) {
      std::cerr << "Warning: could not stat " << dirent.path().string()
                << ", skipping\n";
      continue;
    }
    if (std::filesystem::is_symlink(symStat)) {
      std::cerr << "Warning: skipping symlink " << dirent.path().string()
                << " (mode 120000 not yet implemented)\n";
      continue;
    }

    if (dirent.is_directory()) {
      auto subtree = buildSubtree(dirent.path());
      if (!subtree)
        continue; // empty dir: git doesn't track it
      entries.emplace_back("40000", name, *subtree);
    } else if (dirent.is_regular_file()) {
      std::string content = readFileBytes(dirent.path());
      Blob blob(content);
      ObjectId blobId = store_.write(blob);

      std::string mode = isExecutable(dirent.path()) ? "100755" : "100644";
      entries.emplace_back(mode, name, blobId);
    }
  }

  if (entries.empty()) {
    return std::nullopt;
  }

  Tree tree(entries);
  tree.sortEntries();
  return store_.write(tree);
}

ObjectId TreeBuilder::writeTreeFromDirectory(const std::filesystem::path &dir) {
  auto result = buildSubtree(dir);
  if (result) {
    return *result;
  }
  // Root with nothing in it -- still needs an actual empty-tree object
  // written, not just an in-memory "empty" signal.
  Tree emptyTree({});
  return store_.write(emptyTree);
}
