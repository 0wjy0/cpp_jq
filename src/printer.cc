// cpp_jq - SPDX-License-Identifier: MIT
#include "printer.hpp"
#include <iostream>
namespace cpp_jq {
void print_json(std::ostream& os, const J& v, bool compact) {
    os << (compact ? v.dump() : v.dump(2)) << "\n";
}
}