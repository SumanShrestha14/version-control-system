#pragma once
#include "git_object.h"

class Blob : public GitObject {
public:
    explicit Blob(std::string content) : content_(std::move(content)) {}

    ObjectType type() const override { return ObjectType::Blob; }
    std::string serialize() const override { return content_; }

    const std::string& content() const { return content_; }

    static Blob parse(const std::string& storeBytes);

private:
    std::string content_;
};
