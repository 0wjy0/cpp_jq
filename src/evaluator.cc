// cpp_jq - SPDX-License-Identifier: MIT
#include "cpp_jq/ast.hpp"
#include <type_traits>
#include <algorithm>
#include <cmath>

namespace cpp_jq {

static bool is_optional(const Node& n) {
    return std::visit([](auto& k) -> bool {
        using T = std::decay_t<decltype(k)>;
        if constexpr (std::is_same_v<T, FieldAccess>) return k.optional;
        else if constexpr (std::is_same_v<T, Index>)  return k.optional;
        else if constexpr (std::is_same_v<T, Iterate>) return k.optional;
        else return false;
    }, n.kind);
}

void Identity::eval(const J& in, Values& out) const { out.push_back(in); }
void Literal::eval(const J&, Values& out) const { out.push_back(value); }

void FieldAccess::eval(const J& in, Values& out) const {
    if (!in.is_object()) {
        if (!optional) throw CppJqError({}, "field on non-object");
        return;
    }
    auto it = in.find(name);
    if (it == in.end()) {
        if (!optional) throw CppJqError({}, "no field " + name);
        return;
    }
    out.push_back(*it);
}

void Index::eval(const J& in, Values& out) const {
    if (in.is_array()) {
        int64_t n = static_cast<int64_t>(in.size());
        auto get = [&](int64_t i) -> J {
            if (i < 0) i += n;
            if (i < 0 || i >= n) {
                if (!optional) throw CppJqError({}, "index out of range");
                return J(nullptr);
            }
            return in[i];
        };
        if (has_end) {
            int64_t s = idx, e = end;
            if (s < 0) s += n;
            if (e < 0) e += n;
            if (s < 0) s = 0;
            if (e > n) e = n;
            for (int64_t k = s; k < e; ++k) out.push_back(in[k]);
            return;
        }
        out.push_back(get(idx));
        return;
    }
    if (in.is_object()) {
        out.push_back(in[std::to_string(idx)]);
        return;
    }
    if (!optional) throw CppJqError({}, "index on non-array/object");
}

void Iterate::eval(const J& in, Values& out) const {
    if (in.is_array()) {
        for (auto& v : in) out.push_back(v);
    } else if (in.is_object()) {
        for (auto it = in.begin(); it != in.end(); ++it) out.push_back(it.value());
    } else {
        if (!optional) throw CppJqError({}, "iterate on non-array/object");
    }
}

void Recurse::eval(const J& in, Values& out) const {
    out.push_back(in);
    if (in.is_array()) {
        for (auto& v : in) {
            Values tmp;
            Recurse{inner}.eval(v, tmp);
            for (auto& x : tmp) out.push_back(std::move(x));
        }
    } else if (in.is_object()) {
        for (auto it = in.begin(); it != in.end(); ++it) {
            Values tmp;
            Recurse{inner}.eval(it.value(), tmp);
            for (auto& x : tmp) out.push_back(std::move(x));
        }
    }
}

void Pipe::eval(const J& in, Values& out) const {
    Values mid = lhs->eval(in);
    for (auto& v : mid) {
        Values tmp = rhs->eval(v);
        for (auto& x : tmp) out.push_back(std::move(x));
    }
}

void Comma::eval(const J& in, Values& out) const {
    Values a = lhs->eval(in);
    Values b = rhs->eval(in);
    for (auto& v : a) out.push_back(std::move(v));
    for (auto& v : b) out.push_back(std::move(v));
}

void Group::eval(const J& in, Values& out) const {
    Values tmp = inner->eval(in);
    for (auto& v : tmp) out.push_back(std::move(v));
}

void IfElse::eval(const J& in, Values& out) const {
    Values c = cond->eval(in);
    NodePtr branch = c.empty() ? else_br : then_br;
    Values tmp = branch->eval(in);
    for (auto& v : tmp) out.push_back(std::move(v));
}

void ArrayCtor::eval(const J&, Values& out) const {
    J arr = J::array();
    for (auto& it : items) {
        Values tmp = it->eval(J(nullptr));
        if (tmp.size() != 1) throw CppJqError({}, "array element must be single");
        arr.push_back(tmp[0]);
    }
    out.push_back(arr);
}

void ObjectCtor::eval(const J&, Values& out) const {
    J obj = J::object();
    for (auto& [k, v] : pairs) {
        Values kt = k->eval(J(nullptr));
        Values vt = v->eval(J(nullptr));
        if (kt.size() != 1 || vt.size() != 1) throw CppJqError({}, "object k/v must be single");
        std::string key = kt[0].is_string()
            ? kt[0].get<std::string>()
            : kt[0].dump();
        obj[key] = vt[0];
    }
    out.push_back(obj);
}

void BinOp::eval(const J&, Values&) const { throw CppJqError({}, "BinOp: not implemented in Phase 2"); }
void UnaryOp::eval(const J&, Values&) const { throw CppJqError({}, "UnaryOp: not implemented in Phase 2"); }
void Call::eval(const J&, Values&) const { throw CppJqError({}, "Call: not implemented in Phase 2"); }

Values Node::eval(const J& in) const {
    Values out;
    bool opt = is_optional(*this);
    try {
        std::visit([&](auto& k) { k.eval(in, out); }, kind);
    } catch (const CppJqError&) {
        if (!opt) throw;
        // swallow optional errors
    }
    return out;
}

}