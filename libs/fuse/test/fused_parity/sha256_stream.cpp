#include "fused_parity/sha256_stream.h"

#include <cstring>

namespace rtbot::fused_parity {

namespace {

constexpr std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

}  // namespace

Sha256Stream::Sha256Stream()
    : h_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19},
      buffer_{}, bitlen_(0), buflen_(0) {}

void Sha256Stream::update(double v) {
  update(reinterpret_cast<const std::uint8_t*>(&v), sizeof(double));
}

void Sha256Stream::update(std::int64_t v) {
  update(reinterpret_cast<const std::uint8_t*>(&v), sizeof(std::int64_t));
}

void Sha256Stream::update(const std::uint8_t* bytes, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    buffer_[buflen_++] = bytes[i];
    if (buflen_ == 64) {
      transform();
      bitlen_ += 512;
      buflen_ = 0;
    }
  }
}

void Sha256Stream::transform() {
  std::uint32_t m[64];
  for (int i = 0, j = 0; i < 16; ++i, j += 4) {
    m[i] = (std::uint32_t(buffer_[j]) << 24) |
           (std::uint32_t(buffer_[j + 1]) << 16) |
           (std::uint32_t(buffer_[j + 2]) << 8) |
           std::uint32_t(buffer_[j + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    std::uint32_t s0 = rotr(m[i - 15], 7) ^ rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
    std::uint32_t s1 = rotr(m[i - 2], 17) ^ rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
    m[i] = m[i - 16] + s0 + m[i - 7] + s1;
  }

  std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
  std::uint32_t e = h_[4], f = h_[5], g = h_[6], hh = h_[7];

  for (int i = 0; i < 64; ++i) {
    std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    std::uint32_t ch = (e & f) ^ (~e & g);
    std::uint32_t t1 = hh + S1 + ch + kK[i] + m[i];
    std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    std::uint32_t t2 = S0 + maj;
    hh = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  h_[0] += a;
  h_[1] += b;
  h_[2] += c;
  h_[3] += d;
  h_[4] += e;
  h_[5] += f;
  h_[6] += g;
  h_[7] += hh;
}

std::string Sha256Stream::finalize() {
  std::uint64_t total_bits = bitlen_ + std::uint64_t(buflen_) * 8;

  buffer_[buflen_++] = 0x80;
  if (buflen_ > 56) {
    while (buflen_ < 64) buffer_[buflen_++] = 0;
    transform();
    buflen_ = 0;
  }
  while (buflen_ < 56) buffer_[buflen_++] = 0;
  for (int i = 7; i >= 0; --i) {
    buffer_[buflen_++] = static_cast<std::uint8_t>(total_bits >> (i * 8));
  }
  transform();

  static const char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (std::uint32_t word : h_) {
    for (int i = 3; i >= 0; --i) {
      std::uint8_t byte = static_cast<std::uint8_t>(word >> (i * 8));
      out.push_back(kHex[byte >> 4]);
      out.push_back(kHex[byte & 0xF]);
    }
  }
  return out;
}

}  // namespace rtbot::fused_parity
