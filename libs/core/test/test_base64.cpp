#include <catch2/catch.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

#include "rtbot/Base64.h"

using namespace rtbot;

SCENARIO("Base64 roundtrip encoding and decoding", "[base64]") {
  SECTION("Empty bytes") {
    std::vector<uint8_t> input = {};
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Single byte") {
    std::vector<uint8_t> input = {0x42};
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Two bytes") {
    std::vector<uint8_t> input = {0xAB, 0xCD};
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Three bytes (no padding)") {
    std::vector<uint8_t> input = {0x01, 0x02, 0x03};
    auto encoded = bytes_to_base64(input);
    REQUIRE(encoded.find('=') == std::string::npos);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("All byte values 0-255") {
    std::vector<uint8_t> input(256);
    for (int i = 0; i < 256; i++) {
      input[i] = static_cast<uint8_t>(i);
    }
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("All zeros") {
    std::vector<uint8_t> input(100, 0x00);
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("All 0xFF") {
    std::vector<uint8_t> input(100, 0xFF);
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Large payload") {
    std::vector<uint8_t> input(10000);
    for (size_t i = 0; i < input.size(); i++) {
      input[i] = static_cast<uint8_t>(i % 256);
    }
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Known encoding - Hello") {
    std::vector<uint8_t> input = {72, 101, 108, 108, 111};
    auto encoded = bytes_to_base64(input);
    REQUIRE(encoded == "SGVsbG8=");
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Known encoding - Man") {
    std::vector<uint8_t> input = {77, 97, 110};
    auto encoded = bytes_to_base64(input);
    REQUIRE(encoded == "TWFu");
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Known encoding - Ma") {
    std::vector<uint8_t> input = {77, 97};
    auto encoded = bytes_to_base64(input);
    REQUIRE(encoded == "TWE=");
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Known encoding - M") {
    std::vector<uint8_t> input = {77};
    auto encoded = bytes_to_base64(input);
    REQUIRE(encoded == "TQ==");
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Known encoding - empty string") {
    std::vector<uint8_t> input = {};
    auto encoded = bytes_to_base64(input);
    REQUIRE(encoded == "");
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Four bytes") {
    std::vector<uint8_t> input = {0xDE, 0xAD, 0xBE, 0xEF};
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Five bytes") {
    std::vector<uint8_t> input = {0x01, 0x23, 0x45, 0x67, 0x89};
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Six bytes (two full triplets)") {
    std::vector<uint8_t> input = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    auto encoded = bytes_to_base64(input);
    REQUIRE(encoded.find('=') == std::string::npos);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Seven bytes") {
    std::vector<uint8_t> input = {1, 2, 3, 4, 5, 6, 7};
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Alternating 0x00 and 0xFF") {
    std::vector<uint8_t> input(50);
    for (size_t i = 0; i < input.size(); i++) {
      input[i] = (i % 2 == 0) ? 0x00 : 0xFF;
    }
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Alternating 0xFF and 0x00") {
    std::vector<uint8_t> input(51);
    for (size_t i = 0; i < input.size(); i++) {
      input[i] = (i % 2 == 0) ? 0xFF : 0x00;
    }
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Single zero byte") {
    std::vector<uint8_t> input = {0x00};
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Single max byte") {
    std::vector<uint8_t> input = {0xFF};
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Powers of two") {
    std::vector<uint8_t> input = {1, 2, 4, 8, 16, 32, 64, 128};
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Descending byte values") {
    std::vector<uint8_t> input(256);
    for (int i = 0; i < 256; i++) {
      input[i] = static_cast<uint8_t>(255 - i);
    }
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Repeated pattern 0xAA") {
    std::vector<uint8_t> input(33, 0xAA);
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Repeated pattern 0x55") {
    std::vector<uint8_t> input(34, 0x55);
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Size 1000") {
    std::vector<uint8_t> input(1000);
    for (size_t i = 0; i < input.size(); i++) {
      input[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
    }
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Size 1001 (mod 3 == 2)") {
    std::vector<uint8_t> input(1001);
    for (size_t i = 0; i < input.size(); i++) {
      input[i] = static_cast<uint8_t>((i * 11 + 3) % 256);
    }
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Size 1002 (mod 3 == 0)") {
    std::vector<uint8_t> input(1002);
    for (size_t i = 0; i < input.size(); i++) {
      input[i] = static_cast<uint8_t>((i * 3 + 17) % 256);
    }
    auto encoded = bytes_to_base64(input);
    REQUIRE(encoded.find('=') == std::string::npos);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Double encode-decode roundtrip") {
    std::vector<uint8_t> input = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    auto encoded1 = bytes_to_base64(input);
    auto decoded1 = base64_to_bytes(encoded1);
    auto encoded2 = bytes_to_base64(decoded1);
    auto decoded2 = base64_to_bytes(encoded2);
    REQUIRE(encoded1 == encoded2);
    REQUIRE(decoded2 == input);
  }

  SECTION("Encoding a double value as bytes") {
    double value = 3.14159265358979;
    std::vector<uint8_t> input(reinterpret_cast<const uint8_t*>(&value),
                               reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
    double restored;
    std::memcpy(&restored, decoded.data(), sizeof(restored));
    REQUIRE(restored == value);
  }

  SECTION("Encoding a uint64_t as bytes") {
    uint64_t value = 0xDEADBEEFCAFEBABE;
    std::vector<uint8_t> input(reinterpret_cast<const uint8_t*>(&value),
                               reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
    uint64_t restored;
    std::memcpy(&restored, decoded.data(), sizeof(restored));
    REQUIRE(restored == value);
  }

  SECTION("Encoding multiple integers as bytes") {
    std::vector<uint8_t> input;
    for (int i = 0; i < 10; i++) {
      int32_t val = i * 1000 - 5000;
      auto ptr = reinterpret_cast<const uint8_t*>(&val);
      input.insert(input.end(), ptr, ptr + sizeof(val));
    }
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Encoding is deterministic") {
    std::vector<uint8_t> input = {10, 20, 30, 40, 50};
    auto encoded1 = bytes_to_base64(input);
    auto encoded2 = bytes_to_base64(input);
    REQUIRE(encoded1 == encoded2);
  }

  SECTION("Different inputs produce different encodings") {
    std::vector<uint8_t> input1 = {1, 2, 3};
    std::vector<uint8_t> input2 = {1, 2, 4};
    auto encoded1 = bytes_to_base64(input1);
    auto encoded2 = bytes_to_base64(input2);
    REQUIRE(encoded1 != encoded2);
  }

  SECTION("Encoded length is correct") {
    for (size_t len = 0; len <= 20; len++) {
      std::vector<uint8_t> input(len, 0x42);
      auto encoded = bytes_to_base64(input);
      size_t expected_len = ((len + 2) / 3) * 4;
      REQUIRE(encoded.size() == expected_len);
    }
  }

  SECTION("Padding count is correct") {
    // len % 3 == 0 -> no padding
    std::vector<uint8_t> in3(3, 0x42);
    REQUIRE(bytes_to_base64(in3).substr(bytes_to_base64(in3).size() - 1) != "=");

    // len % 3 == 1 -> two padding chars
    std::vector<uint8_t> in1(1, 0x42);
    auto enc1 = bytes_to_base64(in1);
    REQUIRE(enc1.size() >= 2);
    REQUIRE(enc1[enc1.size() - 1] == '=');
    REQUIRE(enc1[enc1.size() - 2] == '=');

    // len % 3 == 2 -> one padding char
    std::vector<uint8_t> in2(2, 0x42);
    auto enc2 = bytes_to_base64(in2);
    REQUIRE(enc2[enc2.size() - 1] == '=');
    REQUIRE(enc2[enc2.size() - 2] != '=');
  }

  SECTION("50000 bytes stress test") {
    std::vector<uint8_t> input(50000);
    for (size_t i = 0; i < input.size(); i++) {
      input[i] = static_cast<uint8_t>((i * 31 + 97) % 256);
    }
    auto encoded = bytes_to_base64(input);
    auto decoded = base64_to_bytes(encoded);
    REQUIRE(decoded == input);
  }

  SECTION("Encoded string contains only valid base64 characters") {
    std::vector<uint8_t> input(500);
    for (size_t i = 0; i < input.size(); i++) {
      input[i] = static_cast<uint8_t>(i % 256);
    }
    auto encoded = bytes_to_base64(input);
    for (char c : encoded) {
      bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
      REQUIRE(valid);
    }
  }
}
