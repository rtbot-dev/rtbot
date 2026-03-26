#ifndef BASE64_H
#define BASE64_H

#include <string>
#include <vector>
#include <cstdint>

namespace rtbot {

inline std::string bytes_to_base64(const std::vector<uint8_t>& bytes) {
  static constexpr char table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve(((bytes.size() + 2) / 3) * 4);

  size_t i = 0;
  for (; i + 2 < bytes.size(); i += 3) {
    uint32_t n = (uint32_t(bytes[i]) << 16) | (uint32_t(bytes[i + 1]) << 8) | bytes[i + 2];
    result += table[(n >> 18) & 0x3F];
    result += table[(n >> 12) & 0x3F];
    result += table[(n >> 6) & 0x3F];
    result += table[n & 0x3F];
  }
  if (i + 1 == bytes.size()) {
    uint32_t n = uint32_t(bytes[i]) << 16;
    result += table[(n >> 18) & 0x3F];
    result += table[(n >> 12) & 0x3F];
    result += '=';
    result += '=';
  } else if (i + 2 == bytes.size()) {
    uint32_t n = (uint32_t(bytes[i]) << 16) | (uint32_t(bytes[i + 1]) << 8);
    result += table[(n >> 18) & 0x3F];
    result += table[(n >> 12) & 0x3F];
    result += table[(n >> 6) & 0x3F];
    result += '=';
  }
  return result;
}

inline std::vector<uint8_t> base64_to_bytes(const std::string& encoded) {
  static constexpr uint8_t decode_table[] = {
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
      52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
      64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
      15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
      64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
      41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
  };

  std::vector<uint8_t> result;
  result.reserve((encoded.size() / 4) * 3);

  uint32_t buf = 0;
  int bits = 0;
  for (char c : encoded) {
    if (c == '=' || c == '\n' || c == '\r') continue;
    uint8_t val = decode_table[static_cast<uint8_t>(c)];
    if (val == 64) continue;
    buf = (buf << 6) | val;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      result.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
    }
  }
  return result;
}

}  // namespace rtbot

#endif
