#include "commit_tree_command.h"
#include "../core/commit.h"
#include "../core/git_timestamp.h"
#include <iostream>

int CommitTreeCommand::execute() {
    try {
        ObjectId treeId = ObjectId::fromHex(treeSha_);
        std::vector<ObjectId> parents;
        if (!parentSha_.empty()) {
            parents.push_back(ObjectId::fromHex(parentSha_));
        }

        const std::string authorLine = "Suman <suman@example.com> " + GitTimestamp::now();
        Commit commit(treeId, parents, authorLine, authorLine, message_);

        ObjectId id = repo_.objects().write(commit);
        std::cout << id.hex() << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
