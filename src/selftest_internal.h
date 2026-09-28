#pragma once

#include <string>
#include <utility>

// Shared between selftest_checks.cpp (live checks against the real system)
// and selftest_fixtures.cpp (known input -> exact expected output), and
// consumed by the METRICS table in selftest.cpp. Internal to the self-test
// implementation, not part of vitals' public API — see selftest.h for that.

enum class Status { Ok, Skip, Fail };

struct Result {
    Status      status = Status::Ok;
    std::string reason;
};

// inline: this header is included by more than one .cpp, and each of these
// needs a single definition shared across them rather than one per TU.
inline Result ok()                  { return {Status::Ok,   ""}; }
inline Result skip(std::string why) { return {Status::Skip, std::move(why)}; }
inline Result fail(std::string why) { return {Status::Fail, std::move(why)}; }
