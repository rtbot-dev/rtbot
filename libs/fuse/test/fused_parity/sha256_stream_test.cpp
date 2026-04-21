#include <catch2/catch.hpp>

#include "fused_parity/sha256_stream.h"

using rtbot::fused_parity::Sha256Stream;

SCENARIO("sha256 of empty stream is deterministic", "[sha256_stream]") {
  Sha256Stream a, b;
  REQUIRE(a.finalize() == b.finalize());
}

SCENARIO("sha256 of same doubles in same order is equal", "[sha256_stream]") {
  Sha256Stream a, b;
  a.update(1.0);
  a.update(2.0);
  a.update(3.0);
  b.update(1.0);
  b.update(2.0);
  b.update(3.0);
  REQUIRE(a.finalize() == b.finalize());
}

SCENARIO("sha256 is order-sensitive", "[sha256_stream]") {
  Sha256Stream a, b;
  a.update(1.0);
  a.update(2.0);
  b.update(2.0);
  b.update(1.0);
  REQUIRE(a.finalize() != b.finalize());
}

SCENARIO("sha256 empty-input digest matches the canonical SHA-256 empty digest",
         "[sha256_stream][vector]") {
  Sha256Stream h;
  // Known digest of empty input.
  REQUIRE(h.finalize() ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

SCENARIO("sha256 of the three ASCII bytes 'abc' matches the canonical digest",
         "[sha256_stream][vector]") {
  Sha256Stream h;
  const std::uint8_t data[3] = {'a', 'b', 'c'};
  h.update(data, 3);
  REQUIRE(h.finalize() ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
