// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include "value.hpp"
#include "error.hpp"

namespace cpp_jq {

struct Node;

struct Identity          { void eval(const J&, Values&) const; };
struct Empty             { void eval(const J&, Values&) const; };
struct Literal           { J value; void eval(const J&, Values&) const; };
struct FieldAccess       { std::string name; bool optional = false; void eval(const J&, Values&) const; };
struct Index             { int64_t idx = 0; bool has_end = false; int64_t end = 0; bool optional = false; void eval(const J&, Values&) const; };
struct Iterate           { bool optional = false; void eval(const J&, Values&) const; };
struct Recurse           { std::shared_ptr<Node> inner; void eval(const J&, Values&) const; };
struct Pipe              { std::shared_ptr<Node> lhs, rhs; void eval(const J&, Values&) const; };
struct Comma             { std::shared_ptr<Node> lhs, rhs; void eval(const J&, Values&) const; };
struct Group             { std::shared_ptr<Node> inner; void eval(const J&, Values&) const; };
struct IfElse            { std::shared_ptr<Node> cond, then_br, else_br; void eval(const J&, Values&) const; };
struct ArrayCtor         { std::vector<std::shared_ptr<Node>> items; void eval(const J&, Values&) const; };
struct ObjectCtor        { std::vector<std::pair<std::shared_ptr<Node>, std::shared_ptr<Node>>> pairs; void eval(const J&, Values&) const; };
struct BinOp             { std::string op; std::shared_ptr<Node> lhs, rhs; void eval(const J&, Values&) const; };
struct UnaryOp           { std::string op; std::shared_ptr<Node> inner; void eval(const J&, Values&) const; };
struct Call              { std::string name; std::vector<std::shared_ptr<Node>> args; void eval(const J&, Values&) const; bool optional = false; };

struct Node {
    std::variant<Identity, Empty, Literal, FieldAccess, Index, Iterate, Recurse,
                 Pipe, Comma, Group, IfElse, ArrayCtor, ObjectCtor,
                 BinOp, UnaryOp, Call> kind;
    Pos pos;
    Values eval(const J& in) const;
};

using NodePtr = std::shared_ptr<Node>;

}