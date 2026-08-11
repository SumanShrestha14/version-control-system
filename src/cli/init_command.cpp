#include "init_command.h"
#include <iostream>

int InitCommand::execute() {
    try {
        Repository::init(root_);
        std::cout << "Initialized synk directory\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
