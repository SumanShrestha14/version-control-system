#include "ls_tree_command.h"
#include "../core/tree.h"
#include <iostream>

int LsTreeCommand::execute() {
    try {
        ObjectId id = ObjectId::fromHex(sha1_);
        auto obj = repo_.objects().read(id);
        Tree* tree = dynamic_cast<Tree*>(obj.get());
        if (!tree) {
            std::cerr << "fatal: " << sha1_ << " is not a tree object\n";
            return EXIT_FAILURE;
        }

        for (const auto& entry : tree->entries()) {
            if (nameOnly_) {
                std::cout << entry.name() << '\n';
            } else {
                std::cout << entry.paddedMode() << ' ' << entry.typeName()
                          << ' ' << entry.sha().hex() << '\t' << entry.name() << '\n';
            }
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
