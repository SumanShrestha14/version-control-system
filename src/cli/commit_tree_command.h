#pragma once
#include "command.h"
#include "../repository/repository.h"
#include <string>

class CommitTreeCommand : public Command {
public:
    CommitTreeCommand(Repository& repo, std::string treeSha, std::string parentSha, std::string message)
        : repo_(repo), treeSha_(std::move(treeSha)), parentSha_(std::move(parentSha)), message_(std::move(message)) {}
    int execute() override;

private:
    Repository& repo_;
    std::string treeSha_;
    std::string parentSha_;
    std::string message_;
};
