#include "tree_entry.h"

std::string TreeEntry::typeName() const {
    if (isDirectory()) return "tree";
    if (isSubmodule()) return "commit";
    return "blob";
}

std::string TreeEntry::paddedMode() const {
    std::string padded = mode_;
    while (padded.size() < 6) padded = "0" + padded;
    return padded;
}

std::string TreeEntry::encode() const {
    return mode_ + " " + name_ + '\0' + sha_.raw();
}
