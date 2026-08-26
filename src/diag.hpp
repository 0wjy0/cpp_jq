// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <iosfwd>
#include "cpp_jq/error.hpp"
namespace cpp_jq {
void print_diag(std::ostream& os, const Pos& p, const std::string& msg);
}