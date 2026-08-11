#include "clone_command.h"
#include "../net/clone_operation.h"
#include <iostream>

int CloneCommand::execute() {
  try {
    CloneOperation op(url_, targetDir_);
    op.run();
    return EXIT_SUCCESS;
  } catch (const std::exception &e) {
    std::cerr << "fatal: clone failed: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
}
