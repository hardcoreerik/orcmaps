#pragma once

#include <cstdio>

// Minimal, dependency-free host test harness. Deliberately not a real
// framework (no discovery magic, no fixtures) -- see docs/DEPENDENCY_LEDGER.md
// for why a test library wasn't added for this small a surface.

namespace orcmap::test {
inline int g_failures = 0;
}  // namespace orcmap::test

#define ORCMAP_EXPECT_TRUE(cond)                                            \
  do {                                                                      \
    if (!(cond)) {                                                          \
      std::fprintf(stderr, "FAIL %s:%d: expected true: %s\n", __FILE__,     \
                    __LINE__, #cond);                                       \
      ++orcmap::test::g_failures;                                           \
    }                                                                       \
  } while (0)

#define ORCMAP_EXPECT_EQ(a, b)                                              \
  do {                                                                      \
    auto va = (a);                                                         \
    auto vb = (b);                                                         \
    if (!(va == vb)) {                                                     \
      std::fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__,    \
                    #a, #b);                                                \
      ++orcmap::test::g_failures;                                           \
    }                                                                       \
  } while (0)

#define ORCMAP_EXPECT_NEAR(a, b, eps)                                       \
  do {                                                                      \
    double va = static_cast<double>(a);                                    \
    double vb = static_cast<double>(b);                                    \
    double diff = va > vb ? va - vb : vb - va;                             \
    if (diff > (eps)) {                                                     \
      std::fprintf(stderr, "FAIL %s:%d: %s (%f) not within %f of %s (%f)\n",\
                    __FILE__, __LINE__, #a, va, (double)(eps), #b, vb);      \
      ++orcmap::test::g_failures;                                           \
    }                                                                       \
  } while (0)
