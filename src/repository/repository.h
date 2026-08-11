#pragma once
#include "../storage/object_store.h"
#include "ref_store.h"
#include <filesystem>

class Repository {
public:
    // Creates a brand-new .synk layout at `root` (equivalent to `synk init`).
    // Throws if `root/.synk` already exists.
    static Repository init(const std::filesystem::path& root);

    // Opens an existing repo rooted at `root`. Throws if `root/.synk` is missing.
    // (No upward directory search yet -- that's a `Repository::discover()`
    // we can add later if a command needs to run from a subdirectory.)
    static Repository open(const std::filesystem::path& root);

    ObjectStore& objects() { return objectStore_; }
    RefStore& refs() { return refStore_; }
    const std::filesystem::path& root() const { return root_; }
    const std::filesystem::path& synkDir() const { return synkDir_; }

private:
    Repository(std::filesystem::path root, std::filesystem::path synkDir);

    std::filesystem::path root_;
    std::filesystem::path synkDir_;
    ObjectStore objectStore_;
    RefStore refStore_;
};
