// cpp_jq - SPDX-License-Identifier: MIT
#include "builtin.hpp"
#include <unordered_map>
#include <map>
#include <algorithm>

namespace cpp_jq {

namespace builtins {
void length(const BuiltinCtx&, Values&);
void keys(const BuiltinCtx&, Values&);
void type(const BuiltinCtx&, Values&);
void has(const BuiltinCtx&, Values&);
void contains(const BuiltinCtx&, Values&);
void in_(const BuiltinCtx&, Values&);
void map(const BuiltinCtx&, Values&);
void add(const BuiltinCtx&, Values&);
void min_(const BuiltinCtx&, Values&);
void max_(const BuiltinCtx&, Values&);
void sort_(const BuiltinCtx&, Values&);
void unique(const BuiltinCtx&, Values&);
void group_by(const BuiltinCtx&, Values&);
void tostring(const BuiltinCtx&, Values&);
void tonumber(const BuiltinCtx&, Values&);
void fromjson(const BuiltinCtx&, Values&);
}

namespace {
std::unordered_map<std::string, BuiltinFn>& registry() {
    static std::unordered_map<std::string, BuiltinFn> r;
    return r;
}
}

void register_builtins() {
    auto& r = registry();
    r["length"]    = &builtins::length;
    r["keys"]      = &builtins::keys;
    r["type"]      = &builtins::type;
    r["has"]       = &builtins::has;
    r["contains"]  = &builtins::contains;
    r["in"]        = &builtins::in_;
    r["map"]       = &builtins::map;
    r["add"]       = &builtins::add;
    r["min"]       = &builtins::min_;
    r["max"]       = &builtins::max_;
    r["sort"]      = &builtins::sort_;
    r["unique"]    = &builtins::unique;
    r["group_by"]  = &builtins::group_by;
    r["tostring"]  = &builtins::tostring;
    r["tonumber"]  = &builtins::tonumber;
    r["fromjson"]  = &builtins::fromjson;
}

std::unordered_map<std::string, BuiltinFn>& builtin_registry() { return registry(); }

namespace builtins {

void length(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if      (v.is_array())  out.push_back((int64_t)v.size());
        else if (v.is_object()) out.push_back((int64_t)v.size());
        else if (v.is_string()) out.push_back((int64_t)v.get<std::string>().size());
        else if (v.is_number()) out.push_back((int64_t)v.get<double>());
        else throw CppJqError({}, "length: invalid type");
    }
}

void keys(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (!v.is_object()) throw CppJqError({}, "keys: not object");
        J arr = J::array();
        for (auto it = v.begin(); it != v.end(); ++it) arr.push_back(it.key());
        out.push_back(arr);
    }
}

void type(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if      (v.is_null())   out.push_back("null");
        else if (v.is_boolean())out.push_back("boolean");
        else if (v.is_number()) out.push_back("number");
        else if (v.is_string()) out.push_back("string");
        else if (v.is_array())  out.push_back("array");
        else if (v.is_object()) out.push_back("object");
    }
}

void has(const BuiltinCtx& c, Values& out) {
    if (c.pre_args.empty()) throw CppJqError({}, "has: missing arg");
    const J& a = c.pre_args[0];
    for (auto& v : c.in_vals) {
        if (v.is_object()) {
            std::string key = a.is_string() ? a.get<std::string>() : a.dump();
            out.push_back(v.contains(key));
        } else if (v.is_array()) {
            if (!a.is_number()) { out.push_back(false); continue; }
            int64_t i = static_cast<int64_t>(a.get<double>());
            int64_t n = static_cast<int64_t>(v.size());
            if (i < 0) i += n;
            out.push_back(i >= 0 && i < n);
        } else {
            out.push_back(false);
        }
    }
}

void contains(const BuiltinCtx& c, Values& out) {
    if (c.pre_args.empty()) throw CppJqError({}, "contains: missing arg");
    for (auto& v : c.in_vals) {
        const J& target = c.pre_args[0];
        bool r = false;
        if (v.is_array()) {
            for (auto& x : v) if (x == target) { r = true; break; }
        } else {
            r = (v == target);
        }
        out.push_back(r);
    }
}

void in_(const BuiltinCtx& c, Values& out) {
    if (c.pre_args.empty()) throw CppJqError({}, "in: missing arg");
    for (auto& v : c.in_vals) {
        const J& container = c.pre_args[0];
        bool r = false;
        if (container.is_array()) {
            for (auto& x : container) if (x == v) { r = true; break; }
        } else if (container.is_object()) {
            r = container.contains(v.is_string() ? v.get<std::string>() : v.dump());
        } else r = (container == v);
        out.push_back(r);
    }
}

void map(const BuiltinCtx& c, Values& out) {
    // map(f) requires AST arg, not yet implemented in MVP.
    // map(.) is identity.
    for (auto& v : c.in_vals) out.push_back(v);
}

void add(const BuiltinCtx& c, Values& out) {
    Values stream;
    for (auto& v : c.in_vals) {
        if (v.is_array()) for (auto& x : v) stream.push_back(x);
        else stream.push_back(v);
    }
    if (stream.empty()) { out.push_back(J(nullptr)); return; }
    J acc = stream[0];
    for (size_t k = 1; k < stream.size(); ++k) {
        const J& b = stream[k];
        if (acc.is_number() && b.is_number()) acc = acc.get<double>() + b.get<double>();
        else if (acc.is_string() && b.is_string()) acc = acc.get<std::string>() + b.get<std::string>();
        else if (acc.is_array() && b.is_array()) {
            J m = acc; for (auto& x : b) m.push_back(x); acc = m;
        } else if (acc.is_object() && b.is_object()) {
            J m = acc; for (auto it = b.begin(); it != b.end(); ++it) m[it.key()] = it.value(); acc = m;
        } else throw CppJqError({}, "add: type mismatch");
    }
    out.push_back(acc);
}

static int cmp(const J& a, const J& b) {
    if (a.is_number() && b.is_number()) return (a.get<double>() > b.get<double>()) - (a.get<double>() < b.get<double>());
    if (a.is_string() && b.is_string()) return (a.get<std::string>() > b.get<std::string>()) - (a.get<std::string>() < b.get<std::string>());
    return 0;
}

void min_(const BuiltinCtx& c, Values& out) {
    Values stream;
    for (auto& v : c.in_vals) {
        if (v.is_array()) for (auto& x : v) stream.push_back(x);
        else stream.push_back(v);
    }
    if (stream.empty()) { out.push_back(J(nullptr)); return; }
    J best = stream[0];
    for (size_t k = 1; k < stream.size(); ++k) if (cmp(stream[k], best) < 0) best = stream[k];
    out.push_back(best);
}

void max_(const BuiltinCtx& c, Values& out) {
    Values stream;
    for (auto& v : c.in_vals) {
        if (v.is_array()) for (auto& x : v) stream.push_back(x);
        else stream.push_back(v);
    }
    if (stream.empty()) { out.push_back(J(nullptr)); return; }
    J best = stream[0];
    for (size_t k = 1; k < stream.size(); ++k) if (cmp(stream[k], best) > 0) best = stream[k];
    out.push_back(best);
}

void sort_(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (!v.is_array()) throw CppJqError({}, "sort: not array");
        J arr = v;
        std::stable_sort(arr.begin(), arr.end(), [](const J& a, const J& b){ return cmp(a,b) < 0; });
        out.push_back(arr);
    }
}

void unique(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (!v.is_array()) throw CppJqError({}, "unique: not array");
        J arr = J::array();
        for (auto& x : v) {
            bool seen = false;
            for (auto& y : arr) if (y == x) { seen = true; break; }
            if (!seen) arr.push_back(x);
        }
        out.push_back(arr);
    }
}

void group_by(const BuiltinCtx& c, Values& out) {
    if (c.pre_args.empty()) throw CppJqError({}, "group_by requires arg");
    for (auto& v : c.in_vals) {
        if (!v.is_array()) throw CppJqError({}, "group_by: not array");
        std::string key = c.pre_args[0].is_string() ? c.pre_args[0].get<std::string>() : c.pre_args[0].dump();
        std::map<std::string, J> groups;
        for (auto& item : v) {
            std::string k2;
            if (item.is_object()) {
                auto it = item.find(key);
                k2 = (it == item.end()) ? std::string{} : (it->is_string() ? it->get<std::string>() : it->dump());
            } else k2 = item.dump();
            groups[k2].push_back(item);
        }
        J arr = J::array();
        for (auto& kv : groups) arr.push_back(kv.second);
        out.push_back(arr);
    }
}

void tostring(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (v.is_string()) out.push_back(v.get<std::string>());
        else out.push_back(v.dump());
    }
}

void tonumber(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (v.is_number()) { out.push_back(v); continue; }
        if (v.is_string()) {
            try { out.push_back(std::stod(v.get<std::string>())); continue; }
            catch (...) { throw CppJqError({}, "tonumber: invalid string"); }
        }
        throw CppJqError({}, "tonumber: invalid type");
    }
}

void fromjson(const BuiltinCtx& c, Values& out) {
    for (auto& v : c.in_vals) {
        if (!v.is_string()) throw CppJqError({}, "fromjson: not string");
        try {
            out.push_back(J::parse(v.get<std::string>()));
        } catch (const nlohmann::json::parse_error&) {
            throw CppJqError({}, "fromjson: invalid json");
        }
    }
}

}

}