#pragma once
#include "../repository/repository.h"
#include "command.h"
#include <filesystem>

class InitCommand : public Command {
public:
  explicit InitCommand(std::filesystem::path root) : root_(std::move(root)) {}
  int execute() override;

private:
  std::filesystem::path root_;
};
