#include "utils/command.h"
#include <iostream>
#include <string>

using namespace std;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "No command provided.\n";
    return EXIT_FAILURE;
  }

  string command = argv[1];

  if (command == "init") {
    return git_init();
  } else if (command == "cat-file") {
    if (argc != 4) {
      cerr << "Usage: git cat-file -p <sha1>\n";
      return EXIT_FAILURE;
    }
    string flag = argv[2];
    string sha1 = argv[3];
    return cat_file(sha1, flag);
  } else if (command == "hash-object") {
    if (argc < 3 || argc > 4) {
      cerr << "Usage: git hash-object <flag>(optional) <file>\n";
      return EXIT_FAILURE;
    }

    bool write = false;

    if (argc == 3) {
      string file_path = argv[2];
      return hash_object(file_path, write);
    } else {
      string flag = argv[2];
      if (flag == "-w") {
        write = true;
      } else {
        cerr << "Unknown flag: " << flag << '\n';
        return EXIT_FAILURE;
      }
      string file_path = argv[3];
      return hash_object(file_path, write);
    }
  } else if (command == "ls-tree") {
    bool name_only = false;
    std::string sha1;

    std::string arg2 = (argc > 2) ? argv[2] : "";
    std::string arg3 = (argc > 3) ? argv[3] : "";

    if (arg2 == "--name-only") {
      name_only = true;
      sha1 = arg3;
    } else {
      sha1 = arg2;
    }

    if (sha1.empty()) {
      std::cerr << "usage: ls-tree [--name-only] <tree-sha>\n";
      return EXIT_FAILURE;
    }
    return ls_tree(sha1, name_only);
  } else if (command == "write-tree") {
    return write_tree();
  } else if (command == "commit-tree") {
    if (argc < 4) {
      cerr << "Usage: git commit-tree <tree_sha> [-p <parent_sha>] -m "
              "<message>\n";
      return EXIT_FAILURE;
    }

    std::string tree_sha = argv[2];
    std::string parent_sha;
    std::string message;
    bool got_message = false;

    for (int i = 3; i < argc; ++i) {
      std::string arg =
          argv[i]; // convert first -- raw pointer compares bite you otherwise
      if (arg == "-p") {
        if (i + 1 >= argc) {
          cerr << "Error: -p requires a commit SHA\n";
          return EXIT_FAILURE;
        }
        parent_sha = argv[++i];
      } else if (arg == "-m") {
        if (i + 1 >= argc) {
          cerr << "Error: -m requires a message\n";
          return EXIT_FAILURE;
        }
        message = argv[++i];
        got_message = true;
      } else {
        cerr << "Unknown argument: " << arg << '\n';
        return EXIT_FAILURE;
      }
    }

    if (!got_message) {
      cerr << "Error: commit message (-m) is required\n";
      return EXIT_FAILURE;
    }

    return commit_tree(tree_sha, parent_sha, message);
  } else {
    cerr << "Unknown command " << command << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
