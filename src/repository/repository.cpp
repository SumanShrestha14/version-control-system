#include "repository.h"
#include <stdexcept>
#include <fstream>

Repository::Repository(std::filesystem::path root, std::filesystem::path synkDir)
    : root_(std::move(root)),
      synkDir_(std::move(synkDir)),
      objectStore_(synkDir_),
      refStore_(synkDir_) {}

Repository Repository::init(const std::filesystem::path& root) {
    std::filesystem::path synkDir = root / ".synk";
    if (std::filesystem::exists(synkDir)) {
        throw std::runtime_error("Repository::init: .synk already exists at " + root.string());
    }

    std::filesystem::create_directories(synkDir / "objects");
    std::filesystem::create_directories(synkDir / "refs" / "heads");

    Repository repo(root, synkDir);
    repo.refs().setHeadToBranch("main");   // default branch before any commit exists
    return repo;
}

Repository Repository::open(const std::filesystem::path& root) {
    std::filesystem::path synkDir = root / ".synk";
    if (!std::filesystem::exists(synkDir)) {
        throw std::runtime_error("Repository::open: not a synk repository (or any parent): " +
                                  root.string());
    }
    return Repository(root, synkDir);
}
