#include "git_object.h"
#include "blob.h"
#include "commit.h"
#include "tree.h"
#include <stdexcept>

std::string GitObject::typeName() const {
  switch (type()) {
  case ObjectType::Blob:
    return "blob";
  case ObjectType::Tree:
    return "tree";
  case ObjectType::Commit:
    return "commit";
  case ObjectType::Tag:
    return "tag";
  }
  throw std::runtime_error("GitObject::typeName: unknown type");
}

std::string GitObject::header() const {
  std::string body = serialize();
  return typeName() + " " + std::to_string(body.size()) + '\0';
}

std::string GitObject::storeBytes() const { return header() + serialize(); }

ObjectId GitObject::hash() const { return ObjectId::hashOf(storeBytes()); }

std::string GitObject::stripHeader(const std::string &storeBytes,
                                   const std::string &expectedType) {
  std::size_t nullPos = storeBytes.find('\0');
  if (nullPos == std::string::npos) {
    throw std::runtime_error("Malformed object: missing header terminator");
  }
  std::string typeWord = storeBytes.substr(0, storeBytes.find(' '));
  if (typeWord != expectedType) {
    throw std::runtime_error("Object type mismatch: expected " + expectedType +
                             ", got " + typeWord);
  }
  return storeBytes.substr(nullPos + 1);
}

std::unique_ptr<GitObject> GitObject::parse(const std::string &storeBytes) {
  std::string typeWord = storeBytes.substr(0, storeBytes.find(' '));
  if (typeWord == "blob")
    return std::make_unique<Blob>(Blob::parse(storeBytes));
  if (typeWord == "tree")
    return std::make_unique<Tree>(Tree::parse(storeBytes));
  if (typeWord == "commit")
    return std::make_unique<Commit>(Commit::parse(storeBytes));
  throw std::runtime_error("GitObject::parse: unknown object type '" +
                           typeWord + "'");
}
