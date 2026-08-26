// cpp_jq - SPDX-License-Identifier: MIT
#include "cpp_jq/ast.hpp"
namespace cpp_jq {

void Identity::eval(const J& in, Values& out) const { out.push_back(in); }
void Literal::eval(const J&, Values& out) const { out.push_back(value); }
void FieldAccess::eval(const J&, Values&)  const { throw CppJqError({}, "FieldAccess: not implemented"); }
void Index::eval(const J&, Values&)        const { throw CppJqError({}, "Index: not implemented"); }
void Iterate::eval(const J&, Values&)      const { throw CppJqError({}, "Iterate: not implemented"); }
void Recurse::eval(const J&, Values&)      const { throw CppJqError({}, "Recurse: not implemented"); }
void Pipe::eval(const J&, Values&)         const { throw CppJqError({}, "Pipe: not implemented"); }
void Comma::eval(const J&, Values&)        const { throw CppJqError({}, "Comma: not implemented"); }
void Group::eval(const J&, Values&)        const { throw CppJqError({}, "Group: not implemented"); }
void IfElse::eval(const J&, Values&)       const { throw CppJqError({}, "IfElse: not implemented"); }
void ArrayCtor::eval(const J&, Values&)    const { throw CppJqError({}, "ArrayCtor: not implemented"); }
void ObjectCtor::eval(const J&, Values&)   const { throw CppJqError({}, "ObjectCtor: not implemented"); }
void BinOp::eval(const J&, Values&)        const { throw CppJqError({}, "BinOp: not implemented"); }
void UnaryOp::eval(const J&, Values&)      const { throw CppJqError({}, "UnaryOp: not implemented"); }
void Call::eval(const J&, Values&)         const { throw CppJqError({}, "Call: not implemented"); }

Values Node::eval(const J& in) const {
    Values out;
    std::visit([&](auto& k){ k.eval(in, out); }, kind);
    return out;
}

}