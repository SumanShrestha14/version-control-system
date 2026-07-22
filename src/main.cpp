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
  } else {
    cerr << "Unknown command " << command << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
