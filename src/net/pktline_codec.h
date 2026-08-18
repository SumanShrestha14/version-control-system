#pragma once
#include <string>
#include <vector>

struct PktLine {  //structure 
    bool isFlush;
    std::string data;
};

class PktLineCodec {
public:
    static std::vector<PktLine> parse(const std::string& data);
    static std::string format(const std::string& payload);

    PktLineCodec() = delete;   // stateless utility
};
