// cpp_jq - SPDX-License-Identifier: MIT
#include "cpp_jq/ast.hpp"
#include "builtin.hpp"
#include <type_traits>
#include <algorithm>
#include <cmath>
#include <climits>

namespace cpp_jq {

static bool is_optional(const Node& n) {
    return std::visit([](auto& k) -> bool {
        using T = std::decay_t<decltype(k)>;
        if constexpr (std::is_same_v<T, FieldAccess>) return k.optional;
        else if constexpr (std::is_same_v<T, Index>)  return k.optional;
        else if constexpr (std::is_same_v<T, Iterate>) return k.optional;
        else if constexpr (std::is_same_v<T, Call>)    return k.optional;
        else return false;
    }, n.kind);
}

void Identity::eval(const J& in, Values& out) const { out.push_back(in); }
void Empty::eval(const J&, Values&) const {}
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
            if (e == INT64_MAX) e = n;
            else if (e < 0) e += n;
            if (s < 0) s = 0;
            if (e > n) e = n;
            J arr = J::array();
            for (int64_t k = s; k < e; ++k) arr.push_back(in[k]);
            out.push_back(arr);
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
    bool truthy = false;
    for (auto& v : c) {
        if (v.is_boolean()) { if (v.get<bool>()) truthy = true; }
        else truthy = true;
        break;
    }
    NodePtr branch = truthy ? then_br : else_br;
    Values tmp = branch->eval(in);
    for (auto& v : tmp) out.push_back(std::move(v));
}

void ArrayCtor::eval(const J& in, Values& out) const {
    J arr = J::array();
    for (auto& it : items) {
        Values tmp = it->eval(in);
        for (auto& v : tmp) arr.push_back(v);
    }
    out.push_back(arr);
}

void ObjectCtor::eval(const J& in, Values& out) const {
    J obj = J::object();
    for (auto& [k, v] : pairs) {
        Values kt = k->eval(in);
        Values vt = v->eval(in);
        if (kt.size() != 1) throw CppJqError({}, "object key must be single");
        std::string key = kt[0].is_string()
            ? kt[0].get<std::string>()
            : kt[0].dump();
        for (auto& val : vt) obj[key] = val;
    }
    out.push_back(obj);
}

void BinOp::eval(const J& in, Values& out) const {
    Values a = lhs->eval(in);
    Values b = rhs->eval(in);
    if (a.size() != 1 || b.size() != 1) throw CppJqError({}, "binary op: not single");
    const J& x = a[0];
    const J& y = b[0];
    if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
        bool eq = (x == y);
        if (op == "==") { out.push_back(eq); return; }
        if (op == "!=") { out.push_back(!eq); return; }
        int c = 0;
        if (x.is_number() && y.is_number()) {
            double xd = x.get<double>(), yd = y.get<double>();
            c = (xd > yd) - (xd < yd);
        } else if (x.is_string() && y.is_string()) {
            std::string xs = x.get<std::string>();
            std::string ys = y.get<std::string>();
            c = (xs > ys) - (xs < ys);
        } else {
            throw CppJqError({}, "comparison: type mismatch");
        }
        if      (op == "<")  out.push_back(c < 0);
        else if (op == "<=") out.push_back(c <= 0);
        else if (op == ">")  out.push_back(c > 0);
        else if (op == ">=") out.push_back(c >= 0);
        return;
    }
    auto numop = [&](auto f) {
        if (!x.is_number() || !y.is_number()) throw CppJqError({}, "arithmetic on non-number");
        out.push_back(f(x.get<double>(), y.get<double>()));
    };
    if (op == "+") {
        if (x.is_string() && y.is_string()) { out.push_back(x.get<std::string>() + y.get<std::string>()); return; }
        if (x.is_array()  && y.is_array())  { J r = x; for (auto& v : y) r.push_back(v); out.push_back(r); return; }
        if (x.is_object() && y.is_object()) { J r = x; for (auto it = y.begin(); it != y.end(); ++it) r[it.key()] = it.value(); out.push_back(r); return; }
        if (x.is_null() || y.is_null())    { out.push_back(J(nullptr)); return; }
        numop([](double a, double b){ return a + b; });
    } else if (op == "-") { numop([](double a, double b){ return a - b; }); }
      else if (op == "*") {
        if (x.is_string() && y.is_number()) {
            std::string s; int64_t n = (int64_t)y.get<double>();
            for (int64_t k = 0; k < std::abs(n); ++k) s += x.get<std::string>();
            out.push_back(s); return;
        }
        numop([](double a, double b){ return a * b; });
    } else if (op == "/") {
        numop([](double a, double b){ if (b == 0) throw CppJqError({}, "division by zero"); return a / b; });
    } else if (op == "%") {
        numop([](double a, double b){ if (b == 0) throw CppJqError({}, "modulo by zero"); return std::fmod(a, b); });
    } else {
        throw CppJqError({}, "unknown binop: " + op);
    }
}

void UnaryOp::eval(const J& in, Values& out) const {
    Values v = inner->eval(in);
    if (v.size() != 1) throw CppJqError({}, "unary op: not single");
    if (op == "not") {
        out.push_back(!v[0].is_boolean() ? false : !v[0].get<bool>());
    } else if (op == "-") {
        if (!v[0].is_number()) throw CppJqError({}, "negate: not number");
        out.push_back(-v[0].get<double>());
    } else {
        throw CppJqError({}, "unknown unary: " + op);
    }
}

void Call::eval(const J& in, Values& out) const {
    auto& reg = builtin_registry();
    auto it = reg.find(name);
    if (it == reg.end()) throw CppJqError({}, "unknown function: " + name);
    std::vector<J> pre_args;
    for (auto& a : args) {
        Values tmp = a->eval(J(nullptr));
        if (tmp.size() != 1) throw CppJqError({}, "arg must produce single value");
        pre_args.push_back(tmp[0]);
    }
    Values in_vals = { in };
    BuiltinCtx ctx{ in_vals, pre_args };
    it->second(ctx, out);
}

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