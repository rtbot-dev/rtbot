#ifndef RTBOT_FUSED_PARITY_SHA256_STREAM_H
#define RTBOT_FUSED_PARITY_SHA256_STREAM_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace rtbot::fused_parity {

// Minimal SHA256 streaming digest. Used to hash output streams (doubles +
// timestamps) for golden regression testing. Endian policy: host endianness —
// parity is compared on-host, not across hosts.
class Sha256Stream {
 public:
  Sha256Stream();

  void update(double v);
  void update(std::int64_t v);
  void update(const std::uint8_t* bytes, std::size_t n);

  // Returns lowercase hex digest. After calling finalize(), the instance
  // must not be reused.
  std::string finalize();

 private:
  std::array<std::uint32_t, 8> h_;
  std::array<std::uint8_t, 64> buffer_;
  std::uint64_t bitlen_;
  std::size_t buflen_;

  void transform();
};

}  // namespace rtbot::fused_parity

#endif  // RTBOT_FUSED_PARITY_SHA256_STREAM_H
