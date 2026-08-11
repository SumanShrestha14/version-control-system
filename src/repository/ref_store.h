#pragma once
#include "../core/object_id.h"
#include <filesystem>
#include <string>

class RefStore {
public:
    // synkDir is the repo's ".synk" directory.
    explicit RefStore(std::filesystem::path synkDir);

    // refName like "refs/heads/main"
    void writeRef(const std::string& refName, const ObjectId& id);
    ObjectId readRef(const std::string& refName) const;
    bool refExists(const std::string& refName) const;

    // Points HEAD at a branch: writes "ref: refs/heads/<branch>\n"
    void setHeadToBranch(const std::string& branchName);

    // Resolves HEAD -> whatever ref it points at -> that ref's ObjectId.
    // Throws if HEAD is detached-but-unsupported or the target ref is missing.
    ObjectId resolveHead() const;

    // Returns just the branch name HEAD currently points at, e.g. "main",
    // by stripping the "refs/heads/" prefix from HEAD's contents.
    std::string currentBranch() const;

private:
    std::filesystem::path synkDir_;
    std::filesystem::path refPath(const std::string& refName) const;
};
