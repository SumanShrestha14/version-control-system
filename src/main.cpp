#include <iostream>
#include <string>
#include "utils/command.h"

using namespace std;

string decompress(const string &compressed);
int main(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "No command provided.\n";
    return EXIT_FAILURE;
  }

  string command = argv[1];

  if (command == "init") {
    git_init();
  } else if (command == "cat-file") {
    if (argc != 4) {
      cerr << "Usage: git cat-file -p <sha1>\n";
      return EXIT_FAILURE;
    }
    string flag = argv[2];
    string sha1 = argv[3];
    cat_file(sha1, flag);
  } else {
    cerr << "Unknown command " << command << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
