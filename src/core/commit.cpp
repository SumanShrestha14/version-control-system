#include "commit.h"
#include <sstream>
#include <stdexcept>

std::string Commit::serialize() const {
  std::string body = "tree " + tree_.hex() + "\n";
  for (const auto &p : parents_) {
    body += "parent " + p.hex() + "\n";
  }
  body += "author " + author_ + "\n";
  body += "committer " + committer_ + "\n";
  body += "\n";
  body += message_ + "\n";
  return body;
}

Commit Commit::parse(const std::string &storeBytes) {
  std::string body = stripHeader(storeBytes, "commit");

  ObjectId tree;
  std::vector<ObjectId> parents;
  std::string author, committer, message;

  std::istringstream stream(body);
  std::string line;
  while (std::getline(stream, line) && !line.empty()) {
    if (line.rfind("tree ", 0) == 0) {
      tree = ObjectId::fromHex(line.substr(5, 40));
    } else if (line.rfind("parent ", 0) == 0) {
      parents.push_back(ObjectId::fromHex(line.substr(7, 40)));
    } else if (line.rfind("author ", 0) == 0) {
      author = line.substr(7);
    } else if (line.rfind("committer ", 0) == 0) {
      committer = line.substr(10);
    }
  }
  // Everything remaining after the blank line is the commit message.
  std::ostringstream msgStream;
  msgStream << stream.rdbuf();
  message = msgStream.str();
  if (!message.empty() && message.back() == '\n')
    message.pop_back();

  if (tree.empty()) {
    throw std::runtime_error("Malformed commit object: missing tree line");
  }
  return Commit(tree, parents, author, committer, message);
}
