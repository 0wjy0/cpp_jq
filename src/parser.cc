// cpp_jq - SPDX-License-Identifier: MIT
#include "parser.hpp"

namespace cpp_jq {

struct Parser {
    const std::vector<Tok>& ts;
    size_t i = 0;
    explicit Parser(const std::vector<Tok>& t) : ts(t) {}

    const Tok& peek() const { return ts[i]; }
    const Tok& peek2() const { return (i + 1 < ts.size()) ? ts[i + 1] : ts.back(); }
    const Tok& eat()        { return ts[i++]; }
    bool accept(TokKind k)   { if (peek().kind == k) { ++i; return true; } return false; }
    const Tok& expect(TokKind k, const char* msg) {
        if (peek().kind != k) throw CppJqError(peek().pos, std::string("expected ") + msg);
        return eat();
    }

    NodePtr parse_expr() { return parse_cmp(); }

NodePtr parse_cmp() {
    NodePtr lhs = parse_and();
    while (peek().kind == TokKind::EQ || peek().kind == TokKind::NEQ
           || peek().kind == TokKind::LT || peek().kind == TokKind::LE
           || peek().kind == TokKind::GT || peek().kind == TokKind::GE) {
        std::string op = eat().text;
        NodePtr rhs = parse_and();
        Pos p = lhs->pos;
        lhs = std::make_shared<Node>(Node{BinOp{op, lhs, rhs}, p});
    }
    return lhs;
}

    NodePtr parse_and() {
        NodePtr lhs = parse_pipe();
        while (peek().kind == TokKind::AND || peek().kind == TokKind::OR) {
            TokKind k = peek().kind; eat();
            NodePtr rhs = parse_pipe();
            Pos p = lhs->pos;
            NodePtr cond = lhs;
            NodePtr t_node;
            NodePtr e_node;
            if (k == TokKind::AND) {
                t_node = rhs;
                e_node = std::make_shared<Node>(Node{Literal{J(false)}, p});
            } else {
                t_node = std::make_shared<Node>(Node{Literal{J(true)}, p});
                e_node = rhs;
            }
            lhs = std::make_shared<Node>(Node{IfElse{cond, t_node, e_node}, p});
        }
        return lhs;
    }

    NodePtr parse_pipe() {
        NodePtr lhs = parse_comma();
        if (accept(TokKind::PIPE)) {
            NodePtr rhs = parse_pipe();
            return std::make_shared<Node>(Node{Pipe{lhs, rhs}, lhs->pos});
        }
        return lhs;
    }

    NodePtr parse_comma() {
        NodePtr lhs = parse_path();
        if (accept(TokKind::COMMA)) {
            NodePtr rhs = parse_comma();
            return std::make_shared<Node>(Node{Comma{lhs, rhs}, lhs->pos});
        }
        return lhs;
    }

    NodePtr parse_path() {
        Pos p = peek().pos;
        if (peek().kind == TokKind::DOT && peek2().kind == TokKind::DOT) {
            eat(); eat();
            NodePtr inner = parse_path();
            return std::make_shared<Node>(Node{Recurse{inner}, p});
        }
        NodePtr base;
        if (accept(TokKind::DOT)) {
            if (peek().kind == TokKind::LBRACKET && peek2().kind == TokKind::RBRACKET) {
                eat(); eat();
                Iterate it;
                base = std::make_shared<Node>(Node{it, p});
            } else if (peek().kind == TokKind::LBRACKET) {
                eat();
                NodePtr s = parse_slice_expr();
                expect(TokKind::RBRACKET, "]");
                bool opt = false;
                if (accept(TokKind::QUESTION)) opt = true;
                Index ix = std::get<Index>(s->kind);
                ix.optional = opt;
                base = std::make_shared<Node>(Node{ix, p});
            } else if (peek().kind == TokKind::RBRACKET) {
                eat();
                Iterate it;
                base = std::make_shared<Node>(Node{it, p});
            } else if (peek().kind == TokKind::IDENT) {
                std::string name = eat().text;
                FieldAccess fa{name, false};
                base = std::make_shared<Node>(Node{fa, p});
            } else {
                base = std::make_shared<Node>(Node{Identity{}, p});
            }
        } else {
            base = parse_term();
        }

        auto wrap = [&](NodePtr new_access) -> NodePtr {
            return std::make_shared<Node>(Node{Pipe{base, new_access}, base->pos});
        };

        while (true) {
            if (peek().kind == TokKind::DOT && peek2().kind != TokKind::DOT) {
                eat();
                NodePtr new_access;
                if (peek().kind == TokKind::LBRACKET && peek2().kind == TokKind::RBRACKET) {
                    eat(); eat();
                    Iterate it;
                    new_access = std::make_shared<Node>(Node{it, base->pos});
                } else if (peek().kind == TokKind::LBRACKET) {
                    eat();
                    NodePtr s = parse_slice_expr();
                    expect(TokKind::RBRACKET, "]");
                    bool opt = false;
                    if (accept(TokKind::QUESTION)) opt = true;
                    Index ix = std::get<Index>(s->kind);
                    ix.optional = opt;
                    new_access = std::make_shared<Node>(Node{ix, base->pos});
                } else if (peek().kind == TokKind::RBRACKET) {
                    eat();
                    Iterate it;
                    new_access = std::make_shared<Node>(Node{it, base->pos});
                } else if (peek().kind == TokKind::IDENT) {
                    std::string name = eat().text;
                    FieldAccess fa{name, false};
                    new_access = std::make_shared<Node>(Node{fa, base->pos});
                } else {
                    throw CppJqError(peek().pos, "expected field name or [ after .");
                }
                base = wrap(new_access);
                continue;
            }
            if (peek().kind == TokKind::LBRACKET && peek2().kind == TokKind::RBRACKET) {
                eat(); eat();
                bool opt = false;
                if (accept(TokKind::QUESTION)) opt = true;
                Iterate it;
                it.optional = opt;
                NodePtr new_access = std::make_shared<Node>(Node{it, base->pos});
                base = wrap(new_access);
                continue;
            }
            if (peek().kind == TokKind::LBRACKET) {
                eat();
                NodePtr s = parse_slice_expr();
                expect(TokKind::RBRACKET, "]");
                bool opt = false;
                if (accept(TokKind::QUESTION)) opt = true;
                Index ix = std::get<Index>(s->kind);
                ix.optional = opt;
                NodePtr new_access = std::make_shared<Node>(Node{ix, base->pos});
                base = wrap(new_access);
                continue;
            }
            if (peek().kind == TokKind::QUESTION) {
                eat();
                if (auto* fa = std::get_if<FieldAccess>(&base->kind)) fa->optional = true;
                else if (auto* ix = std::get_if<Index>(&base->kind)) ix->optional = true;
                else if (auto* it = std::get_if<Iterate>(&base->kind)) it->optional = true;
                else throw CppJqError(peek().pos, "? must follow field/index/iterate");
                continue;
            }
            break;
        }
        return base;
    }

    NodePtr parse_slice_expr() {
        Pos p = peek().pos;
        const Tok& a = peek();
        if (a.kind != TokKind::NUMBER) {
            throw CppJqError(p, "expected number in [..]");
        }
        int64_t idx = static_cast<int64_t>(eat().num);
        if (accept(TokKind::COLON)) {
            Index ix;
            ix.idx = idx;
            ix.has_end = true;
            if (peek().kind == TokKind::NUMBER) {
                ix.end = static_cast<int64_t>(eat().num);
            } else {
                ix.end = 0;
            }
            return std::make_shared<Node>(Node{ix, p});
        }
        Index ix;
        ix.idx = idx;
        return std::make_shared<Node>(Node{ix, p});
    }

    NodePtr parse_term() { return parse_primary(); }

    NodePtr parse_primary() {
        Pos p = peek().pos;
        if (peek().kind == TokKind::NUMBER) {
            std::string txt = eat().text;
            J jv;
            try { jv = J::parse(txt); } catch (...) { jv = J(0); }
            return std::make_shared<Node>(Node{Literal{jv}, p});
        }
        if (peek().kind == TokKind::STRING) {
            std::string s = eat().text;
            return std::make_shared<Node>(Node{Literal{J(s)}, p});
        }
        if (accept(TokKind::TRUE))  return std::make_shared<Node>(Node{Literal{J(true)}, p});
        if (accept(TokKind::FALSE)) return std::make_shared<Node>(Node{Literal{J(false)}, p});
        if (accept(TokKind::NULL_T)) return std::make_shared<Node>(Node{Literal{J(nullptr)}, p});
        if (peek().kind == TokKind::IDENT && peek2().kind == TokKind::LPAREN) {
            std::string name = eat().text;
            eat();
            std::vector<NodePtr> args;
            if (!accept(TokKind::RPAREN)) {
                args.push_back(parse_expr());
                while (accept(TokKind::COMMA)) args.push_back(parse_expr());
                expect(TokKind::RPAREN, ")");
            }
            return std::make_shared<Node>(Node{Call{name, args}, p});
        }
        if (peek().kind == TokKind::IDENT) {
            std::string name = eat().text;
            return std::make_shared<Node>(Node{Call{name, {}}, p});
        }
        if (accept(TokKind::LPAREN)) {
            NodePtr inner = parse_expr();
            expect(TokKind::RPAREN, ")");
            return inner;
        }
        if (accept(TokKind::LBRACKET)) {
            std::vector<NodePtr> items;
            if (!accept(TokKind::RBRACKET)) {
                items.push_back(parse_expr());
                while (accept(TokKind::COMMA)) items.push_back(parse_expr());
                expect(TokKind::RBRACKET, "]");
            }
            return std::make_shared<Node>(Node{ArrayCtor{items}, p});
        }
        if (accept(TokKind::LBRACE)) {
            std::vector<std::pair<NodePtr, NodePtr>> pairs;
            if (!accept(TokKind::RBRACE)) {
                pairs.push_back(parse_pair());
                while (accept(TokKind::COMMA)) pairs.push_back(parse_pair());
                expect(TokKind::RBRACE, "}");
            }
            return std::make_shared<Node>(Node{ObjectCtor{pairs}, p});
        }
        if (accept(TokKind::IF)) {
            NodePtr cond = parse_expr();
            expect(TokKind::THEN, "then");
            NodePtr t_node = parse_expr();
            expect(TokKind::ELSE, "else");
            NodePtr e_node = parse_expr();
            expect(TokKind::END, "end");
            return std::make_shared<Node>(Node{IfElse{cond, t_node, e_node}, p});
        }
        if (peek().kind == TokKind::SELECT) {
            eat();
            expect(TokKind::LPAREN, "(");
            NodePtr inner = parse_expr();
            expect(TokKind::RPAREN, ")");
            NodePtr t_node = std::make_shared<Node>(Node{Identity{}, p});
            NodePtr e_node = std::make_shared<Node>(Node{ArrayCtor{{}}, p});
            return std::make_shared<Node>(Node{IfElse{inner, t_node, e_node}, p});
        }
        if (accept(TokKind::MINUS)) {
            NodePtr inner = parse_primary();
            return std::make_shared<Node>(Node{UnaryOp{"-", inner}, p});
        }
        if (accept(TokKind::NOT)) {
            NodePtr inner = parse_primary();
            return std::make_shared<Node>(Node{UnaryOp{"not", inner}, p});
        }
        throw CppJqError(p, "unexpected token in primary");
    }

    std::pair<NodePtr, NodePtr> parse_pair() {
        Pos p = peek().pos;
        NodePtr k;
        if (peek().kind == TokKind::IDENT && peek2().kind == TokKind::COLON) {
            std::string name = eat().text;
            k = std::make_shared<Node>(Node{Literal{J(name)}, p});
        } else if (peek().kind == TokKind::STRING) {
            std::string s = eat().text;
            k = std::make_shared<Node>(Node{Literal{J(s)}, p});
        } else {
            k = parse_primary();
        }
        expect(TokKind::COLON, ":");
        NodePtr v = parse_path();
        return {k, v};
    }
};

NodePtr parse(const std::vector<Tok>& toks) {
    Parser p(toks);
    NodePtr n = p.parse_expr();
    if (p.peek().kind != TokKind::EOF_T) {
        throw CppJqError(p.peek().pos, "trailing tokens");
    }
    return n;
}

}