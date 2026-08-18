#pragma once //this is for the one time inlude of library file 
#include "object_id.h"
#include <string>

class TreeEntry {
public:
  TreeEntry(std::string mode, std::string name, ObjectId sha)
      : mode_(std::move(mode)), name_(std::move(name)), sha_(sha) {}

  const std::string &mode() const { return mode_; }
  const std::string &name() const { return name_; }
  const ObjectId &sha() const { return sha_; }

  bool isDirectory() const { return mode_ == "40000"; }
  bool isSubmodule() const { return mode_ == "160000"; }
  bool isExecutable() const { return mode_ == "100755"; }

  std::string typeName() const; // "tree" / "commit" / "blob"
  std::string
  paddedMode() const; // zero-padded to 6 chars, for ls-tree -l output

  // Git's real sort order: directory names compare as if they had a
  // trailing '/'. Required for byte-exact match with real git trees.
  std::string sortKey() const { return isDirectory() ? name_ + "/" : name_; }

  // Serialized form: "<mode> <name>\0<20 raw bytes>"
  std::string encode() const;

private:
  std::string mode_;
  std::string name_;
  ObjectId sha_;
};
