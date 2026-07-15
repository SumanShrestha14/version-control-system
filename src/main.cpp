#include <cstdlib>
#include <iostream>
#include <string>
#include "utils/command.h"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "No command provided.\n";
    return EXIT_FAILURE;
  }

  string command = argv[1];

  if (command == "init") {
      if(git_init()){
          cerr << "Successfully initialized repository.\n";
          return EXIT_SUCCESS;
      }else{
          cerr << "Failed to initialize repository.\n";
          return EXIT_FAILURE;
      }
  } else if (command == "cat-file") {
    if (argc != 4) {
      cerr << "Usage: git cat-file -p <sha1>\n";
      return EXIT_FAILURE;
    }
    string flag = argv[2];
    string sha1 = argv[3];
    if(cat_file(sha1, flag)){
        cout<<"Successfully retrieved blob content.\n";
        return EXIT_SUCCESS;
    }else{
        cerr << "Failed to retrieve blob content.\n";
        return EXIT_FAILURE;
    }
  } else {
    cerr << "Unknown command " << command << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
