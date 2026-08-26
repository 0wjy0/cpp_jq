// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <iosfwd>
#include "cpp_jq/value.hpp"
namespace cpp_jq {
void print_json(std::ostream& os, const J& v, bool compact);
}