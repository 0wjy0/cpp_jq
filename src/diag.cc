// cpp_jq - SPDX-License-Identifier: MIT
#include "diag.hpp"
#include <iostream>
namespace cpp_jq {
void print_diag(std::ostream& os, const Pos& p, const std::string& msg) {
    os << "cpp_jq: error";
    if (p.line > 0) os << " at " << p.line << ":" << p.col;
    os << ": " << msg << "\n";
}
}