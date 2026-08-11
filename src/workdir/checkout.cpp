#include "checkout.h"
#include "../core/blob.h"
#include "../core/commit.h"
#include "../core/tree.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

void Checkout::checkoutTree(const ObjectId &treeId,
                            const std::filesystem::path &dest) {
  auto obj = store_.read(treeId);
  Tree *tree = dynamic_cast<Tree *>(obj.get());
  if (!tree) {
    throw std::runtime_error("Checkout: " + treeId.hex() +
                             " is not a tree object");
  }

  for (const auto &entry : tree->entries()) {
    std::filesystem::path entryPath = dest / entry.name();

    if (entry.isDirectory()) {
      std::filesystem::create_directories(entryPath);
      checkoutTree(entry.sha(), entryPath);
    } else if (entry.isSubmodule()) {
      std::cerr << "Warning: skipping submodule " << entry.name()
                << " (gitlink not yet implemented)\n";
      continue;
    } else {
      auto blobObj = store_.read(entry.sha());
      Blob *blob = dynamic_cast<Blob *>(blobObj.get());
      if (!blob) {
        throw std::runtime_error("Checkout: malformed blob " +
                                 entry.sha().hex());
      }

      std::ofstream out(entryPath, std::ios::binary);
      if (!out) {
        throw std::runtime_error("Checkout: failed to write " +
                                 entryPath.string());
      }
      out.write(blob->content().data(),
                static_cast<std::streamsize>(blob->content().size()));
      out.close();

      if (entry.isExecutable()) {
        std::filesystem::permissions(entryPath,
                                     std::filesystem::perms::owner_exec |
                                         std::filesystem::perms::group_exec |
                                         std::filesystem::perms::others_exec,
                                     std::filesystem::perm_options::add);
      }
    }
  }
}

void Checkout::checkoutCommit(const ObjectId &commitId,
                              const std::filesystem::path &dest) {
  auto obj = store_.read(commitId);
  Commit *commit = dynamic_cast<Commit *>(obj.get());
  if (!commit) {
    throw std::runtime_error("Checkout: " + commitId.hex() +
                             " is not a commit object");
  }
  checkoutTree(commit->tree(), dest);
}
