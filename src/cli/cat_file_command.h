#pragma once
#include "command.h"
#include "../repository/repository.h"
#include <string>

class CatFileCommand : public Command {
public:
    CatFileCommand(Repository& repo, std::string flag, std::string sha1)
        : repo_(repo), flag_(std::move(flag)), sha1_(std::move(sha1)) {}
    int execute() override;

private:
    Repository& repo_;
    std::string flag_;
    std::string sha1_;
};
