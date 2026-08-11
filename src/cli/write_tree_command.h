#pragma once
#include "command.h"
#include "../repository/repository.h"

class WriteTreeCommand : public Command {
public:
    explicit WriteTreeCommand(Repository& repo) : repo_(repo) {}
    int execute() override;

private:
    Repository& repo_;
};
