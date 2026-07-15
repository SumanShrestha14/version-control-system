#include "command.h"
#include "utils.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int git_init() {
  try {
    std::filesystem::create_directory(".git");
    std::filesystem::create_directory(".git/objects");
    std::filesystem::create_directory(".git/refs");

    std::ofstream headFile(".git/HEAD");
    if (headFile.is_open()) {
      headFile << "ref: refs/heads/main\n";
      headFile.close();
    } else {
      std::cerr << "Failed to create .git/HEAD file.\n";
      return EXIT_FAILURE;
    }

    std::cout << "Initialized git directory\n";
    return EXIT_SUCCESS;
  } catch (const std::filesystem::filesystem_error &e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  }
}

int cat_file(const std::string &sha1, const std::string &flag) {
  if (sha1.length() != 40) {
    std::cerr << "Invalid SHA-1 hash: " << sha1 << '\n';
    return EXIT_FAILURE;
  }
  if (flag != "-p") {
    std::cerr << "Unknown flag " << flag << '\n';
    return EXIT_FAILURE;
  }

  std::ifstream file(".git/objects/" + sha1.substr(0, 2) + "/" + sha1.substr(2), std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << ".git/objects/" << sha1.substr(0, 2)
              << "/" << sha1.substr(2) << '\n';
    return EXIT_FAILURE;
  }

  std::string compressed((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::string decompressed = decompress(compressed);
  std::size_t null_pos = decompressed.find('\0');
  if (null_pos == std::string::npos) {
    std::cerr << "Malformed object: missing null byte\n";
    return EXIT_FAILURE;
  }

  std::string blob_content = decompressed.substr(null_pos + 1);
  std::cout << blob_content;
  file.close();
  return EXIT_SUCCESS;
}
