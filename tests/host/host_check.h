#pragma once

#include <cstdio>
#include <cstdlib>

// Host tests must not use assert(): -DNDEBUG turns failures into silent success.

[[noreturn]] inline void hostFail(const char* msg) {
  std::fprintf(stderr, "FAIL: %s\n", msg);
  std::abort();
}

inline void hostRequire(bool cond, const char* msg) {
  if (!cond) {
    hostFail(msg);
  }
}
