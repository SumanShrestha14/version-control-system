#include "hash_object_command.h"
#include "../core/blob.h"
#include <iostream>
#include <fstream>

int HashObjectCommand::execute() {
    try {
        std::ifstream file(filePath_, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + filePath_);
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        Blob blob(content);
        ObjectId id = write_ ? repo_.objects().write(blob) : blob.hash();
        std::cout << id.hex() << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
