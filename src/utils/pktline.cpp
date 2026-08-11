#include "pktline.h"
#include <cstdio>
#include <stdexcept>

std::vector<PktLine> parse_pkt_lines(const std::string &data) {
  std::vector<PktLine> lines;
  size_t pos = 0;
  while (pos + 4 <= data.size()) {
    std::string len_hex = data.substr(pos, 4);
    int len = 0;
    for (char c : len_hex) {
      int digit;
      if (c >= '0' && c <= '9')
        digit = c - '0';
      else if (c >= 'a' && c <= 'f')
        digit = c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
        digit = c - 'A' + 10;
      else
        throw std::runtime_error("pkt-line: invalid length header '" + len_hex +
                                 "'");
      len = (len << 4) | digit;
    }

    if (len == 0 || len == 1) { // flush-pkt or delim-pkt
      lines.push_back({true, ""});
      pos += 4;
      continue;
    }
    if (len < 4)
      throw std::runtime_error("pkt-line: invalid length " +
                               std::to_string(len));
    if (pos + len > data.size())
      throw std::runtime_error("pkt-line: length exceeds remaining buffer");

    lines.push_back({false, data.substr(pos + 4, len - 4)});
    pos += len;
  }
  return lines;
}

std::string format_pkt_line(const std::string &payload) {
  if (payload.empty())
    return "0000";
  size_t total_len = payload.size() + 4;
  char buf[5];
  std::snprintf(buf, sizeof(buf), "%04zx", total_len);
  return std::string(buf, 4) + payload;
}
