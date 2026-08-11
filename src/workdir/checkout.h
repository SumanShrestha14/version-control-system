#pragma once
#include "../core/object_id.h"
#include "../storage/object_store.h"
#include <filesystem>

class Checkout {
public:
  explicit Checkout(ObjectStore &store) : store_(store) {}

  // Materializes the given tree's contents as real files/dirs under dest.
  void checkoutTree(const ObjectId &treeId, const std::filesystem::path &dest);

  // Convenience: resolves the commit's tree, then checks that out.
  void checkoutCommit(const ObjectId &commitId,
                      const std::filesystem::path &dest);

private:
  ObjectStore &store_;
};
