// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <stdexcept>
#include <string>

namespace cpp_jq {

struct Pos {
    int line = 1;
    int col  = 1;
};

class CppJqError : public std::runtime_error {
public:
    CppJqError(Pos p, std::string msg)
        : std::runtime_error(msg), pos_(p) {}
    Pos pos() const noexcept { return pos_; }
private:
    Pos pos_;
};

}