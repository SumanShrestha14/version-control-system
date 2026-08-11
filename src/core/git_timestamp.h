#pragma once
#include <string>

class GitTimestamp {
public:
    // Returns "<epoch_seconds> <±HHMM>", git's author/committer timestamp format.
    static std::string now();

    GitTimestamp() = delete;
};
