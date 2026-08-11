#pragma once
#include "command.h"
#include "../repository/repository.h"
#include <string>

class HashObjectCommand : public Command {
public:
    HashObjectCommand(Repository& repo, std::string filePath, bool write)
        : repo_(repo), filePath_(std::move(filePath)), write_(write) {}
    int execute() override;

private:
    Repository& repo_;
    std::string filePath_;
    bool write_;
};
