#pragma once

#include <stddef.h>

// Rewrites supported MCO text transports wherever they occur inside an
// ordinary GROUP_TEXT body. Text outside a recognised transport is copied
// byte-for-byte; binary GROUP_DATA handling remains separate.
namespace mco_compat {

struct Options {
  bool mcotxt;
  bool mcmp;
  bool mcoimg;
};

struct Result {
  bool changed;
  bool truncated;
  size_t length;
};

// [output] is always NUL-terminated when output_capacity is non-zero.
// A token is recognised only at a text boundary and must carry a non-empty
// Base91 run accepted by the corresponding format detector. The longest
// accepted token prefix is consumed, leaving surrounding prose and subsequent
// tokens available to the scanner.
Result transform(const char* input, char* output, size_t output_capacity,
                 const Options& options);

}  // namespace mco_compat
