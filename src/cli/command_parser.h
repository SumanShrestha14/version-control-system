#pragma once
#include "command.h"
#include "../repository/repository.h"
#include <memory>

class CommandParser {
public:
    // repo may be nullptr only when argv[1] is "init" or "clone".
    static std::unique_ptr<Command> parse(int argc, char** argv, Repository* repo);
};
