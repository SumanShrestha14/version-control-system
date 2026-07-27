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

  std::ifstream file(".git/objects/" + sha1.substr(0, 2) + "/" + sha1.substr(2),
                     std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << ".git/objects/" << sha1.substr(0, 2)
              << "/" << sha1.substr(2) << '\n';
    return EXIT_FAILURE;
  }

  std::string compressed((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
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

int hash_object(const std::string &file_path, bool write) {
  try {
    std::string content = read_file(file_path);
    std::string header = "blob " + std::to_string(content.size()) + '\0';
    std::string store = header + content;
    std::string hash = sha1_hex(store);
    if (write) {
      std::string compressed = compress(store);
      write_object_file(hash, compressed);
    }
    std::cout << hash << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
}

int ls_tree(const std::string &sha1, bool name_only) {
  if (sha1.length() != 40) {
    std::cerr << "Invalid SHA-1 hash: " << sha1 << '\n';
    return EXIT_FAILURE;
  }

  std::string decompressed;
  try {
    decompressed = read_object(sha1);
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  }

  // Sanity check: make sure this is actually a tree object, not a blob/commit
  if (decompressed.rfind("tree ", 0) != 0) {
    std::cerr << "fatal: " << sha1 << " is not a tree object\n";
    return EXIT_FAILURE;
  }

  std::vector<TreeEntry> entries;
  try {
    entries = parse_tree_object(decompressed);
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  }

  for (const auto &entry : entries) {
    if (name_only) {
      std::cout << entry.name << '\n';
    } else {
      std::cout << pad_mode(entry.mode) << ' ' << mode_to_type(entry.mode)
                << ' ' << entry.sha1 << '\t' << entry.name << '\n';
    }
  }

  return EXIT_SUCCESS;
}
