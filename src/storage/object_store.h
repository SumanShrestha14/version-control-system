#pragma once
#include "../core/object_id.h"
#include "../core/git_object.h"
#include <filesystem>
#include <memory>
#include <string>

class ObjectStore {
public:
    // synkDir is the repo's ".synk" directory (or equivalent), not ".synk/objects" --
    // this class owns the "objects/xx/yyyy..." layout decision internally.
    explicit ObjectStore(std::filesystem::path synkDir);

    // Serializes obj, hashes it, compresses it, writes it to disk if not
    // already present. Returns the resulting ObjectId either way.
    ObjectId write(const GitObject& obj);

    // Reads + decompresses + parses into the right GitObject subclass.
    std::unique_ptr<GitObject> read(const ObjectId& id) const;

    // Reads + decompresses but does NOT parse -- needed by cat-file, which
    // must print raw content and shouldn't have to reconstruct it from a
    // parsed Blob's serialize() (though for Blob those happen to be identical,
    // this keeps cat-file correct even if a future object type's serialize()
    // ever normalizes anything).
    std::string readRawWithHeader(const ObjectId& id) const;

    bool exists(const ObjectId& id) const;

    // Writes raw pre-built store bytes (used by clone, where objects arrive
    // as already-typed pack entries rather than as GitObject instances).
    ObjectId writeRaw(const std::string& type, const std::string& body);

private:
    std::filesystem::path objectsDir_;
    std::filesystem::path pathFor(const ObjectId& id) const;
};
