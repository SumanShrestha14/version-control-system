#include "ref_store.h"
#include <fstream>
#include <stdexcept>

namespace {
constexpr const char* kHeadsPrefix = "refs/heads/";
}

RefStore::RefStore(std::filesystem::path synkDir) : synkDir_(std::move(synkDir)) {}

std::filesystem::path RefStore::refPath(const std::string& refName) const {
    return synkDir_ / refName;
}

void RefStore::writeRef(const std::string& refName, const ObjectId& id) {
    std::filesystem::path path = refPath(refName);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("RefStore::writeRef: failed to write " + path.string());
    }
    out << id.hex() << "\n";
}

bool RefStore::refExists(const std::string& refName) const {
    return std::filesystem::exists(refPath(refName));
}

ObjectId RefStore::readRef(const std::string& refName) const {
    std::filesystem::path path = refPath(refName);
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("RefStore::readRef: no such ref: " + refName);
    }
    std::string hex;
    std::getline(in, hex);
    return ObjectId::fromHex(hex);
}

void RefStore::setHeadToBranch(const std::string& branchName) {
    std::ofstream head(synkDir_ / "HEAD");
    if (!head) {
        throw std::runtime_error("RefStore::setHeadToBranch: failed to write HEAD");
    }
    head << "ref: " << kHeadsPrefix << branchName << "\n";
}

std::string RefStore::currentBranch() const {
    std::ifstream head(synkDir_ / "HEAD");
    if (!head) {
        throw std::runtime_error("RefStore::currentBranch: failed to read HEAD");
    }
    std::string line;
    std::getline(head, line);
    // Expected form: "ref: refs/heads/<branch>"
    const std::string prefix = std::string("ref: ") + kHeadsPrefix;
    if (line.rfind(prefix, 0) != 0) {
        throw std::runtime_error("RefStore::currentBranch: HEAD is not a branch ref: " + line);
    }
    return line.substr(prefix.size());
}

ObjectId RefStore::resolveHead() const {
    return readRef(kHeadsPrefix + currentBranch());
}
