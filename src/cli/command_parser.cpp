#include "command_parser.h"
#include "cat_file_command.h"
#include "clone_command.h"
#include "commit_tree_command.h"
#include "hash_object_command.h"
#include "init_command.h"
#include "ls_tree_command.h"
#include "write_tree_command.h"
#include <iostream>

namespace {
Repository &requireRepo(Repository *repo, const std::string &commandName) {
  if (!repo) {
    throw std::runtime_error(commandName + ": not a synk repository");
  }
  return *repo;
}
} // namespace

std::unique_ptr<Command> CommandParser::parse(int argc, char **argv,
                                              Repository *repo) {
  if (argc < 2) {
    std::cerr << "No command provided.\n";
    return nullptr;
  }
  std::string command = argv[1];

  if (command == "init") {
    return std::make_unique<InitCommand>(std::filesystem::path("."));
  }

  if (command == "cat-file") {
    if (argc != 4) {
      std::cerr << "Usage: Synk cat-file -p <sha1>\n";
      return nullptr;
    }
    return std::make_unique<CatFileCommand>(requireRepo(repo, "cat-file"),
                                            argv[2], argv[3]);
  }

  if (command == "hash-object") {
    if (argc < 3 || argc > 4) {
      std::cerr << "Usage: Synk hash-object <flag>(optional) <file>\n";
      return nullptr;
    }
    bool write = false;
    std::string filePath;
    if (argc == 3) {
      filePath = argv[2];
    } else {
      std::string flag = argv[2];
      if (flag != "-w") {
        std::cerr << "Unknown flag: " << flag << '\n';
        return nullptr;
      }
      write = true;
      filePath = argv[3];
    }
    return std::make_unique<HashObjectCommand>(requireRepo(repo, "hash-object"),
                                               filePath, write);
  }

  if (command == "ls-tree") {
    bool nameOnly = false;
    std::string sha1;
    std::string arg2 = (argc > 2) ? argv[2] : "";
    std::string arg3 = (argc > 3) ? argv[3] : "";
    if (arg2 == "--name-only") {
      nameOnly = true;
      sha1 = arg3;
    } else {
      sha1 = arg2;
    }
    if (sha1.empty()) {
      std::cerr << "usage: ls-tree [--name-only] <tree-sha>\n";
      return nullptr;
    }
    return std::make_unique<LsTreeCommand>(requireRepo(repo, "ls-tree"), sha1,
                                           nameOnly);
  }

  if (command == "write-tree") {
    return std::make_unique<WriteTreeCommand>(requireRepo(repo, "write-tree"));
  }

  if (command == "commit-tree") {
    if (argc < 4) {
      std::cerr << "Usage: Synk commit-tree <tree_sha> [-p <parent_sha>] -m "
                   "<message>\n";
      return nullptr;
    }
    std::string treeSha = argv[2];
    std::string parentSha, message;
    bool gotMessage = false;

    for (int i = 3; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "-p") {
        if (i + 1 >= argc) {
          std::cerr << "Error: -p requires a commit SHA\n";
          return nullptr;
        }
        parentSha = argv[++i];
      } else if (arg == "-m") {
        if (i + 1 >= argc) {
          std::cerr << "Error: -m requires a message\n";
          return nullptr;
        }
        message = argv[++i];
        gotMessage = true;
      } else {
        std::cerr << "Unknown argument: " << arg << '\n';
        return nullptr;
      }
    }
    if (!gotMessage) {
      std::cerr << "Error: commit message (-m) is required\n";
      return nullptr;
    }
    return std::make_unique<CommitTreeCommand>(requireRepo(repo, "commit-tree"),
                                               treeSha, parentSha, message);
  }

  if (command == "clone") {
    if (argc < 4) {
      std::cerr << "Usage: " << argv[0] << " clone <url> <dir>\n";
      return nullptr;
    }
    return std::make_unique<CloneCommand>(argv[2], argv[3]);
  }

  std::cerr << "Unknown command " << command << '\n';
  return nullptr;
}
