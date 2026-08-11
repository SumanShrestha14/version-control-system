#pragma once
#include "packfile_parser.h"
#include <vector>
#include <string>

struct ResolvedObject {
    PackObjType type;
    std::string data;   // fully reconstructed object body (post-delta-application)
};

class DeltaResolver {
public:
    static std::string applyDelta(const std::string& base, const std::string& delta);
    static std::vector<ResolvedObject> resolve(const std::vector<PackObject>& objects);

    DeltaResolver() = delete;
};
