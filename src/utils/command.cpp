#include "command.h"
#include "http.h"
#include "packfile.h"
#include "pktline.h"
#include "utils.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
namespace {
// Extracts the tree SHA from a commit object's decompressed content.
// read_object() already strips zlib compression but the commit *header*
// ("commit <size>\0") is still present -- skip past that first.
std::string commit_tree_sha(const std::string &commit_sha) {
  std::string decompressed = read_object(commit_sha);

  std::size_t header_end = decompressed.find('\0');
  if (header_end == std::string::npos) {
    throw std::runtime_error(
        "Malformed commit object: missing header terminator");
  }
  std::string body = decompressed.substr(header_end + 1);

  if (body.rfind("tree ", 0) != 0) {
    throw std::runtime_error("Malformed commit object: missing tree line");
  }
  std::size_t newline = body.find('\n');
  if (newline == std::string::npos || newline < 5 + 40) {
    throw std::runtime_error("Malformed commit object: bad tree line");
  }
  return body.substr(5, 40); // "tree " is 5 chars, sha is 40
}

// Recursively walks a tree object and materializes it as real files/dirs
// under `dest_dir`. Reuses parse_tree_object (already validated by ls-tree)
// and read_object (already validated by cat-file) rather than duplicating
// their parsing logic.
void checkout_tree(const std::string &tree_sha,
                   const std::filesystem::path &dest_dir) {
  std::string decompressed = read_object(tree_sha);
  if (decompressed.rfind("tree ", 0) != 0) {
    throw std::runtime_error("checkout: " + tree_sha + " is not a tree object");
  }

  std::vector<TreeEntry> entries = parse_tree_object(decompressed);

  for (const auto &entry : entries) {
    std::filesystem::path entry_path = dest_dir / entry.name;

    if (entry.mode == "40000") {
      std::filesystem::create_directories(entry_path);
      checkout_tree(entry.sha1, entry_path);
    } else if (entry.mode == "160000") {
      // Submodule (gitlink) -- points at another repo's commit, not an
      // object we have. Skipping rather than silently mis-writing it,
      // same "diagnose, don't fake it" approach as the symlink case above.
      std::cerr << "Warning: skipping submodule " << entry.name
                << " (gitlink not yet implemented)\n";
      continue;
    } else {
      // 100644 (regular) or 100755 (executable) -- both are blobs
      std::string blob_decompressed = read_object(entry.sha1);
      std::size_t header_end = blob_decompressed.find('\0');
      if (header_end == std::string::npos) {
        throw std::runtime_error("checkout: malformed blob " + entry.sha1);
      }
      std::string content = blob_decompressed.substr(header_end + 1);

      std::ofstream out(entry_path, std::ios::binary);
      if (!out) {
        throw std::runtime_error("checkout: failed to write " +
                                 entry_path.string());
      }
      out.write(content.data(), static_cast<std::streamsize>(content.size()));
      out.close();

      if (entry.mode == "100755") {
        std::filesystem::permissions(entry_path,
                                     std::filesystem::perms::owner_exec |
                                         std::filesystem::perms::group_exec |
                                         std::filesystem::perms::others_exec,
                                     std::filesystem::perm_options::add);
      }
    }
  }
}
}
namespace {

struct WriteTreeEntry {
  std::string mode;
  std::string name;
  std::string raw_sha; // 20 raw bytes
};

// Git's sort key: directories are compared as if their name had a
// trailing '/'. This only changes ordering vs a plain name sort in
// edge cases (e.g. "foo" dir vs "foo.txt" file), but it's required
// for byte-exact matches with real git.
std::string sort_key(const WriteTreeEntry &e) {
  return e.mode == "40000" ? e.name + "/" : e.name;
}

std::string write_tree_recursive(const std::filesystem::path &dir_path) {
  std::vector<WriteTreeEntry> entries;

  for (const auto &dirent : std::filesystem::directory_iterator(dir_path)) {
    std::string name = dirent.path().filename().string();
    if (name == ".Synk")
      continue;

    // Check for symlinks BEFORE is_directory()/is_regular_file(), since those
    // follow symlinks by default (they use status(), not symlink_status()).
    // Without this check, a symlink to a directory would get silently
    // recursed into, and a symlink to a file would get silently hashed as
    // a regular blob -- both wrong.
    std::error_code ec;
    auto sym_stat = std::filesystem::symlink_status(dirent.path(), ec);
    if (ec) {
      std::cerr << "Warning: could not stat " << dirent.path().string()
                << ", skipping\n";
      continue;
    }
    if (std::filesystem::is_symlink(sym_stat)) {
      // Symlinks (mode 120000) are explicitly skipped for now rather than
      // stored as gitlink/symlink blobs. This is a real gap, not silent
      // mishandling: we diagnose it instead of following the link.
      std::cerr << "Warning: skipping symlink " << dirent.path().string()
                << " (mode 120000 not yet implemented)\n";
      continue;
    }

    if (dirent.is_directory()) {
      std::string subtree_hex = write_tree_recursive(dirent.path());
      if (subtree_hex.empty())
        continue; // empty dir: git doesn't track it
      entries.push_back(WriteTreeEntry{"40000", name, hex_to_raw(subtree_hex)});
    } else if (dirent.is_regular_file()) {
      std::string content = read_file(dirent.path().string());
      std::string header = "blob " + std::to_string(content.size()) + '\0';
      std::string store = header + content;

      std::string hash_hex = sha1_hex(store);
      std::string compressed = compress(store);
      write_object_file(hash_hex, compressed);

      bool is_executable =
          (std::filesystem::status(dirent.path()).permissions() &
           std::filesystem::perms::owner_exec) != std::filesystem::perms::none;
      std::string mode = is_executable ? "100755" : "100644";

      entries.push_back(WriteTreeEntry{mode, name, sha1_raw(store)});
    }
  }

  if (entries.empty()) {
    return ""; // signal "nothing to contribute" to the caller
  }

  std::sort(entries.begin(), entries.end(),
            [](const WriteTreeEntry &a, const WriteTreeEntry &b) {
              return sort_key(a) < sort_key(b);
            });

  std::string content;
  for (const auto &e : entries) {
    content += e.mode + " " + e.name + '\0' + e.raw_sha;
  }

  std::string header = "tree " + std::to_string(content.size()) + '\0';
  std::string store = header + content;

  std::string hash_hex = sha1_hex(store);
  std::string compressed = compress(store);
  write_object_file(hash_hex, compressed);

  return hash_hex;
}

} // namespace

int synk_init() {
  try {
    std::filesystem::create_directory(".synk");
    std::filesystem::create_directory(".synk/objects");
    std::filesystem::create_directory(".synk/refs");

    std::ofstream headFile(".synk/HEAD");
    if (headFile.is_open()) {
      headFile << "ref: refs/heads/main\n";
      headFile.close();
    } else {
      std::cerr << "Failed to create .synk/HEAD file.\n";
      return EXIT_FAILURE;
    }

    std::cout << "Initialized synk directory\n";
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

  std::ifstream file(".synk/objects/" + sha1.substr(0, 2) + "/" +
                         sha1.substr(2),
                     std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << ".synk/objects/"
              << sha1.substr(0, 2) << "/" << sha1.substr(2) << '\n';
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

int write_tree() {
  try {
    std::string hash = write_tree_recursive(".");
    if (hash.empty()) {
      // Root with literally nothing in it -- still needs to actually be
      // written to .synk/objects, not just have its hash computed.
      std::string store = std::string("tree 0") + '\0';
      hash = sha1_hex(store);
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

int commit_tree(const std::string &tree_sha, const std::string &parent_sha,
                const std::string &message) {
  if (tree_sha.length() != 40) {
    std::cerr << "fatal: not a valid tree object: " << tree_sha << '\n';
    return EXIT_FAILURE;
  }
  if (!parent_sha.empty() && parent_sha.length() != 40) {
    std::cerr << "fatal: not a valid commit object: " << parent_sha << '\n';
    return EXIT_FAILURE;
  }

  const std::string author = "Suman <suman@example.com>";
  std::string when = current_synk_timestamp();

  std::string content = "tree " + tree_sha + "\n";
  if (!parent_sha.empty()) {
    content += "parent " + parent_sha + "\n";
  }
  content += "author " + author + " " + when + "\n";
  content += "committer " + author + " " + when + "\n";
  content += "\n";
  content += message + "\n";

  std::string header = "commit " + std::to_string(content.size()) + '\0';
  std::string store = header + content;

  std::string hash = sha1_hex(store);
  std::string compressed = compress(store);
  write_object_file(hash, compressed);

  std::cout << hash << '\n';
  return EXIT_SUCCESS;
}

void handle_clone(const std::string &repo_url, const std::string &target_dir) {
  std::string discovery_url = repo_url + "/info/refs?service=git-upload-pack";

  HttpResponse resp = http_get(discovery_url, {"Git-Protocol: version=0"});
  if (resp.status_code != 200) {
    std::cerr << "Discovery failed, HTTP " << resp.status_code << "\n";
    std::exit(1);
  }

  auto lines = parse_pkt_lines(resp.body);

  std::vector<std::pair<std::string, std::string>> refs;
  std::string head_sha, default_branch;

  for (const auto &line : lines) {
    if (line.is_flush)
      continue;
    if (line.data.rfind("# service=", 0) == 0)
      continue; // service announcement

    std::string payload = line.data;
    if (!payload.empty() && payload.back() == '\n')
      payload.pop_back();

    std::string capabilities;
    size_t nul_pos = payload.find('\0');
    if (nul_pos != std::string::npos) {
      capabilities = payload.substr(nul_pos + 1);
      payload = payload.substr(0, nul_pos);
    }

    size_t space_pos = payload.find(' ');
    if (space_pos == std::string::npos)
      continue;

    std::string sha = payload.substr(0, space_pos);
    std::string ref_name = payload.substr(space_pos + 1);
    refs.emplace_back(sha, ref_name);

    if (ref_name == "HEAD")
      head_sha = sha;

    if (!capabilities.empty()) {
      size_t sym_pos = capabilities.find("symref=HEAD:");
      if (sym_pos != std::string::npos) {
        size_t start = sym_pos + std::string("symref=HEAD:").size();
        size_t end = capabilities.find(' ', start);
        default_branch = capabilities.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
      }
    }
  }

  std::cerr << "Discovered " << refs.size() << " refs:\n";
  for (const auto &[sha, name] : refs)
    std::cerr << "  " << sha << " " << name << "\n";
  std::cerr << "HEAD sha: " << head_sha << "\n";
  std::cerr << "Default branch: "
            << (default_branch.empty() ? "(unknown)" : default_branch) << "\n";

  if (head_sha.empty() || head_sha == std::string(40, '0')) {
    std::cerr << "warning: You appear to have cloned an empty repository.\n";
    std::filesystem::create_directories(target_dir);
    return;
  }

  // --- M2: negotiate and fetch the packfile ---
  std::string upload_pack_url = repo_url + "/git-upload-pack";
  std::string request_body;
  request_body +=
      format_pkt_line("want " + head_sha + " side-band-64k agent=synk/0.1\n");
  request_body += format_pkt_line(""); // flush-pkt
  request_body += format_pkt_line("done\n");

  HttpResponse pack_resp =
      http_post(upload_pack_url, request_body,
                {"Content-Type: application/x-git-upload-pack-request",
                 "Accept: application/x-git-upload-pack-result"});

  if (pack_resp.status_code != 200) {
    std::cerr << "upload-pack request failed, HTTP " << pack_resp.status_code
              << "\n";
    std::exit(1);
  }

  std::cerr << "upload-pack response size: " << pack_resp.body.size()
            << " bytes\n";

  // Demux side-band-64k: the response is pkt-line framed, and (usually)
  // each payload's first byte is a channel indicator:
  //   0x01 = pack data, 0x02 = progress text, 0x03 = fatal error
  auto pack_lines = parse_pkt_lines(pack_resp.body);

  std::string packfile_data;
  bool saw_sideband = false;

  for (const auto &line : pack_lines) {
    if (line.is_flush || line.data.empty())
      continue;

    unsigned char channel = static_cast<unsigned char>(line.data[0]);
    if (channel == 1 || channel == 2 || channel == 3) {
      saw_sideband = true;
      if (channel == 1) {
        packfile_data.append(line.data, 1, std::string::npos);
      } else if (channel == 2) {
        std::cerr << "remote: " << line.data.substr(1);
      } else {
        std::cerr << "fatal (remote): " << line.data.substr(1) << "\n";
        std::exit(1);
      }
    }
  }

  if (!saw_sideband) {
    // No side-band capability negotiated: the whole demuxed pkt-line
    // stream (minus any leading NAK/ACK line) IS the pack, but since we
    // didn't request side-band, some servers may send it as one giant
    // non-pkt-line blob after an initial "NAK\n" line. Handle both.
    std::cerr << "warning: no side-band detected, attempting raw fallback\n";
    size_t pack_start = pack_resp.body.find("PACK");
    if (pack_start == std::string::npos) {
      std::cerr << "fatal: could not locate PACK signature in response\n";
      std::exit(1);
    }
    packfile_data = pack_resp.body.substr(pack_start);
  }

  std::cerr << "Extracted packfile: " << packfile_data.size() << " bytes\n";
  std::cerr << "First 4 bytes: " << packfile_data.substr(0, 4) << "\n";

  // Save raw pack to disk for now so we can inspect/verify it before M3
  std::filesystem::create_directories(target_dir);
  std::ofstream pack_out(target_dir + "/downloaded.pack", std::ios::binary);
  pack_out.write(packfile_data.data(), packfile_data.size());
  pack_out.close();

  std::cerr << "Saved pack to " << target_dir << "/downloaded.pack\n";

  // Set up .synk inside the target directory so write_object_file's
  // relative ".synk/objects/..." paths land in the right place.
  // Must happen AFTER downloaded.pack is saved (which uses a target_dir-
  // relative path) and BEFORE parse_packfile's write loop runs.
  std::filesystem::create_directories(target_dir + "/.synk/objects");
  std::filesystem::create_directories(target_dir + "/.synk/refs");
  std::filesystem::current_path(target_dir);
  // --- M3: parse the pack into individual objects (deltas not yet resolved)
  // ---
  uint32_t header_count = 0;
  std::vector<PackObject> pack_objects;
  try {
    pack_objects = parse_packfile(packfile_data, header_count);
  } catch (const std::exception &e) {
    std::cerr << "fatal: " << e.what() << "\n";
    std::exit(1);
  }

  std::cerr << "Pack header says " << header_count << " objects, parsed "
            << pack_objects.size() << "\n";

  // --- M4/M5: resolve delta objects (ofs-delta and ref-delta) against
  // their bases, walking chains of arbitrary depth ---
  std::vector<ResolvedObject> resolved_objects;
  try {
    resolved_objects = resolve_pack_objects(pack_objects);
  } catch (const std::exception &e) {
    std::cerr << "fatal: " << e.what() << "\n";
    std::exit(1);
  }

  int written = 0;
  for (const auto &obj : resolved_objects) {
    std::string type_name = pack_obj_type_name(obj.type);
    std::string header =
        type_name + " " + std::to_string(obj.data.size()) + '\0';
    std::string store = header + obj.data;

    std::string hash_hex = sha1_hex(store);
    std::string compressed = compress(store);
    write_object_file(hash_hex, compressed);
    written++;
  }

  std::cerr << "Wrote " << written << " objects (all delta chains resolved)\n";

  for (const auto &obj : resolved_objects) {
    std::string type_name = pack_obj_type_name(obj.type);
    std::string header =
        type_name + " " + std::to_string(obj.data.size()) + '\0';
    std::string store = header + obj.data;

    std::string hash_hex = sha1_hex(store);
    std::string compressed = compress(store);
    write_object_file(hash_hex, compressed);
    written++;
  }
  // --- M6: write refs + HEAD ---
  // default_branch looks like "refs/heads/main"; extract just "main" for
  // the file path under .synk/refs/heads/.
  std::string branch_name =
      "main"; // fallback if symref capability wasn't advertised
  if (default_branch.rfind("refs/heads/", 0) == 0) {
    branch_name = default_branch.substr(std::string("refs/heads/").size());
  }

  std::filesystem::create_directories(".synk/refs/heads");
  {
    std::ofstream ref_file(".synk/refs/heads/" + branch_name);
    ref_file << head_sha << "\n";
  }
  {
    std::ofstream head_file(".synk/HEAD");
    head_file << "ref: refs/heads/" << branch_name << "\n";
  }
  std::cerr << "Wrote ref refs/heads/" << branch_name << " -> " << head_sha
            << "\n";

  // --- M7: checkout HEAD commit's tree into the working directory ---
  try {
    std::string tree_sha = commit_tree_sha(head_sha);
    checkout_tree(tree_sha, ".");
    std::cerr << "Checked out working directory from tree " << tree_sha << "\n";
  } catch (const std::exception &e) {
    std::cerr << "fatal: checkout failed: " << e.what() << "\n";
    std::exit(1);
  }
}
