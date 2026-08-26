// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <vector>
#include "cpp_jq/ast.hpp"
#include "lexer.hpp"

namespace cpp_jq {
NodePtr parse(const std::vector<Tok>& toks);
}