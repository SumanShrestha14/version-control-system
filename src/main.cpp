#include "cli/command_parser.h"
#include "repository/repository.h"
#include <curl/curl.h>
#include <iostream>

int main(int argc, char* argv[]) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    std::unique_ptr<Repository> repo;
    bool needsRepo = argc >= 2 &&
                      std::string(argv[1]) != "init" &&
                      std::string(argv[1]) != "clone";

    if (needsRepo) {
        try {
            repo = std::make_unique<Repository>(Repository::open("."));
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            curl_global_cleanup();
            return EXIT_FAILURE;
        }
    }

    auto cmd = CommandParser::parse(argc, argv, repo.get());
    int rc = cmd ? cmd->execute() : EXIT_FAILURE;

    curl_global_cleanup();
    return rc;
}
