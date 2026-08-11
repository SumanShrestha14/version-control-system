#pragma once
#include "../core/object_id.h"
#include "../storage/object_store.h"
#include <filesystem>

class TreeBuilder {
public:
    explicit TreeBuilder(ObjectStore& store) : store_(store) {}

    // Recursively walks `dir`, writing a Blob for each regular file and a
    // Tree for each subdirectory (bottom-up), and returns the ObjectId of
    // the Tree representing `dir` itself. Empty directories are skipped,
    // matching git's behavior of not tracking directories directly.
    ObjectId writeTreeFromDirectory(const std::filesystem::path& dir);

private:
    ObjectStore& store_;

    // Returns std::nullopt if `dir` contributes nothing (empty / all
    // entries skipped), so the caller can decide whether to write an
    // empty-tree object at the root or omit the subtree entirely.
    std::optional<ObjectId> buildSubtree(const std::filesystem::path& dir);
};
