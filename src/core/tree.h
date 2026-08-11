#pragma once
#include "git_object.h"
#include "tree_entry.h"
#include <vector>

class Tree : public GitObject {
public:
  explicit Tree(std::vector<TreeEntry> entries)
      : entries_(std::move(entries)) {}

  ObjectType type() const override { return ObjectType::Tree; }
  std::string serialize() const override; // encodes entries in sorted order

  const std::vector<TreeEntry> &entries() const { return entries_; }
  void addEntry(TreeEntry e) { entries_.push_back(std::move(e)); }
  void sortEntries();

  static Tree parse(const std::string &storeBytes);

private:
  std::vector<TreeEntry> entries_;
};
