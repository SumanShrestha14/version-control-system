#pragma once
#include "command.h"
#include <string>
#include <filesystem>

class CloneCommand : public Command {
public:
    CloneCommand(std::string url, std::filesystem::path targetDir)
        : url_(std::move(url)), targetDir_(std::move(targetDir)) {}
    int execute() override;

private:
    std::string url_;
    std::filesystem::path targetDir_;
};
