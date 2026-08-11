#pragma once
#include "object_id.h"
#include <memory>
#include <string>

enum class ObjectType { Blob, Tree, Commit, Tag };

class GitObject {
public:
  virtual ~GitObject() = default;

  virtual ObjectType type() const = 0;
  virtual std::string
  serialize() const = 0; // body only, no "type size\0" header

  std::string typeName() const;   // "blob" / "tree" / "commit" / "tag"
  std::string header() const;     // "blob 12\0"
  std::string storeBytes() const; // header() + serialize()
  ObjectId hash() const;          // ObjectId::hashOf(storeBytes())

  // Factory: given the type-prefixed decompressed bytes read from disk,
  // constructs the right concrete subclass. This is what replaces the
  // manual "check decompressed.rfind("tree ", 0) == 0" checks scattered
  // through ls_tree/cat_file/commit_tree_sha.
  static std::unique_ptr<GitObject> parse(const std::string &storeBytes);

protected:
  // Subclasses call this from parse() with just the body (header stripped).
  static std::string stripHeader(const std::string &storeBytes,
                                 const std::string &expectedType);
};
