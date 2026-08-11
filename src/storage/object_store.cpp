#include "object_store.h"
#include "compressor.h"
#include <fstream>
#include <stdexcept>

ObjectStore::ObjectStore(std::filesystem::path synkDir)
    : objectsDir_(synkDir / "objects") {}

std::filesystem::path ObjectStore::pathFor(const ObjectId& id) const {
    const std::string& hex = id.hex();
    return objectsDir_ / hex.substr(0, 2) / hex.substr(2);
}

bool ObjectStore::exists(const ObjectId& id) const {
    return std::filesystem::exists(pathFor(id));
}

ObjectId ObjectStore::write(const GitObject& obj) {
    ObjectId id = obj.hash();
    if (exists(id)) return id;   // content-addressed: identical content already stored

    std::filesystem::path filePath = pathFor(id);
    std::filesystem::create_directories(filePath.parent_path());

    std::string compressed = Compressor::compress(obj.storeBytes());
    std::ofstream out(filePath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("ObjectStore::write: failed to open " + filePath.string());
    }
    out.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
    return id;
}

ObjectId ObjectStore::writeRaw(const std::string& type, const std::string& body) {
    std::string store = type + " " + std::to_string(body.size()) + '\0' + body;
    ObjectId id = ObjectId::hashOf(store);
    if (exists(id)) return id;

    std::filesystem::path filePath = pathFor(id);
    std::filesystem::create_directories(filePath.parent_path());

    std::string compressed = Compressor::compress(store);
    std::ofstream out(filePath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("ObjectStore::writeRaw: failed to open " + filePath.string());
    }
    out.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
    return id;
}

std::string ObjectStore::readRawWithHeader(const ObjectId& id) const {
    std::filesystem::path filePath = pathFor(id);
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("ObjectStore::read: failed to open " + filePath.string());
    }
    std::string compressed((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    return Compressor::decompress(compressed);
}

std::unique_ptr<GitObject> ObjectStore::read(const ObjectId& id) const {
    return GitObject::parse(readRawWithHeader(id));
}
