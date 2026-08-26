// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "cpp_jq/ast.hpp"

namespace cpp_jq {

struct BuiltinCtx {
    const std::vector<J>& in_vals;
    const std::vector<J>& pre_args;
};

using BuiltinFn = std::function<void(const BuiltinCtx&, Values&)>;

void register_builtins();
std::unordered_map<std::string, BuiltinFn>& builtin_registry();

}