#include "write_tree_command.h"
#include "../workdir/tree_builder.h"
#include <iostream>

int WriteTreeCommand::execute() {
    try {
        TreeBuilder builder(repo_.objects());
        ObjectId id = builder.writeTreeFromDirectory(repo_.root());
        std::cout << id.hex() << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
