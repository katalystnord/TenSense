#pragma once

// A minimal check harness for the engine's tests: no Qt, no framework, so
// the ts_analysis tests build with exactly what ts_analysis builds with.
//
//   TS_CASE(name_of_the_case) { TS_REQUIRE(p); TS_CHECK(x == 3); TS_NEAR(a, b, tol); }
//   int main() { return ts::test::run(); }

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace ts::test {

struct Case {
    const char *name;
    std::function<void()> body;
};

inline std::vector<Case> &cases()
{
    static std::vector<Case> all;
    return all;
}

inline int &failures()
{
    static int n = 0;
    return n;
}

struct Register {
    Register(const char *name, std::function<void()> body)
    {
        cases().push_back({name, std::move(body)});
    }
};

inline void fail(const char *file, int line, const std::string &what)
{
    std::printf("  FAIL %s:%d: %s\n", file, line, what.c_str());
    ++failures();
}

inline int run()
{
    int failedCases = 0;
    for (const Case &c : cases()) {
        const int before = failures();
        c.body();
        const bool ok = failures() == before;
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok)
            ++failedCases;
    }
    std::printf("%zu cases, %d failed\n", cases().size(), failedCases);
    return failedCases == 0 ? 0 : 1;
}

} // namespace ts::test

#define TS_CASE(name)                                                         \
    static void name();                                                       \
    static ts::test::Register name##_reg(#name, name);                        \
    static void name()

#define TS_CHECK(cond)                                                        \
    do {                                                                      \
        if (!(cond))                                                          \
            ts::test::fail(__FILE__, __LINE__, #cond);                        \
    } while (0)

// Like TS_CHECK, but ends the case on failure: for a precondition the rest
// of the case depends on (an Outcome about to be dereferenced).
#define TS_REQUIRE(cond)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            ts::test::fail(__FILE__, __LINE__, #cond);                        \
            return;                                                           \
        }                                                                     \
    } while (0)

// |actual - expected| <= tol, with both values printed on failure.
#define TS_NEAR(actual, expected, tol)                                        \
    do {                                                                      \
        const double ts_a_ = (actual), ts_e_ = (expected), ts_t_ = (tol);     \
        if (!(std::fabs(ts_a_ - ts_e_) <= ts_t_)) {                           \
            char ts_buf_[256];                                                \
            std::snprintf(ts_buf_, sizeof ts_buf_,                            \
                          "%s = %.10g, expected %.10g within %.3g",           \
                          #actual, ts_a_, ts_e_, ts_t_);                      \
            ts::test::fail(__FILE__, __LINE__, ts_buf_);                      \
        }                                                                     \
    } while (0)
