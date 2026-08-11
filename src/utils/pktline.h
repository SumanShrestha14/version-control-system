#pragma once
#include <string>
#include <vector>

struct PktLine {
    bool is_flush;   // true for 0000 (and 0001 delim, treated the same for our purposes)
    std::string data; // payload, empty if is_flush
};

std::vector<PktLine> parse_pkt_lines(const std::string& data);
std::string format_pkt_line(const std::string& payload); // "" -> flush pkt "0000"
