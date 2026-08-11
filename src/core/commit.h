#pragma once
#include "git_object.h"
#include <vector>
#include <string>

class Commit : public GitObject {
public:
    Commit(ObjectId tree, std::vector<ObjectId> parents,
           std::string author, std::string committer, std::string message)
        : tree_(tree), parents_(std::move(parents)),
          author_(std::move(author)), committer_(std::move(committer)),
          message_(std::move(message)) {}

    ObjectType type() const override { return ObjectType::Commit; }
    std::string serialize() const override;

    const ObjectId& tree() const { return tree_; }
    const std::vector<ObjectId>& parents() const { return parents_; }
    const std::string& message() const { return message_; }

    static Commit parse(const std::string& storeBytes);

private:
    ObjectId tree_;
    std::vector<ObjectId> parents_;
    std::string author_;      // full "Name <email> timestamp tz" line content
    std::string committer_;
    std::string message_;
};
