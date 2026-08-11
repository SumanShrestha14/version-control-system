#pragma once
#include "command.h"
#include "../repository/repository.h"
#include <string>

class LsTreeCommand : public Command {
public:
    LsTreeCommand(Repository& repo, std::string sha1, bool nameOnly)
        : repo_(repo), sha1_(std::move(sha1)), nameOnly_(nameOnly) {}
    int execute() override;

private:
    Repository& repo_;
    std::string sha1_;
    bool nameOnly_;
};
