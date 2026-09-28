#pragma once

#include <string>
#include <utility>

enum class Status { Ok, Skip, Fail };

struct Result {
    Status      status = Status::Ok;
    std::string reason;
};

inline Result ok()                  { return {Status::Ok,   ""}; }
inline Result skip(std::string why) { return {Status::Skip, std::move(why)}; }
inline Result fail(std::string why) { return {Status::Fail, std::move(why)}; }
