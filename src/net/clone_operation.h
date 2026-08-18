//code for clone operation
#pragma once
#include "http_client.h"
#include "../repository/repository.h"
#include <string>
#include <filesystem>
#include <vector>
#include <utility>

class CloneOperation { //class for clone operation 
public:
    CloneOperation(std::string repoUrl, std::filesystem::path targetDir);//target directory
    void run();

private:
    std::string repoUrl_;
    std::filesystem::path targetDir_;
    HttpClient http_;

    struct DiscoveryResult {
        std::vector<std::pair<std::string, std::string>> refs;   // (sha, refName)
        std::string headSha;
        std::string defaultBranch;   // e.g. "refs/heads/main", may be empty
    };

    DiscoveryResult discoverRefs();
    std::string fetchPackfile(const std::string& wantSha);
    std::string extractPackData(const std::string& rawResponse);   // sideband demux + raw fallback
};
