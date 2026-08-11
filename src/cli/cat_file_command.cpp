#include "cat_file_command.h"
#include "../core/object_id.h"
#include <iostream>

int CatFileCommand::execute() {
    if (flag_ != "-p") {
        std::cerr << "Unknown flag " << flag_ << '\n';
        return EXIT_FAILURE;
    }
    try {
        ObjectId id = ObjectId::fromHex(sha1_);
        std::string raw = repo_.objects().readRawWithHeader(id);
        std::size_t nullPos = raw.find('\0');
        if (nullPos == std::string::npos) {
            std::cerr << "Malformed object: missing null byte\n";
            return EXIT_FAILURE;
        }
        std::cout << raw.substr(nullPos + 1);
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
