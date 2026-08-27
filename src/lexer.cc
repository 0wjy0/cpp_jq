// cpp_jq - SPDX-License-Identifier: MIT
#include "lexer.hpp"
#include <cctype>
#include <stdexcept>

namespace cpp_jq {

static bool is_id_start(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
static bool is_id_cont(char c)  { return std::isalnum((unsigned char)c) || c == '_'; }

static bool is_kw(const std::string& s, TokKind& k) {
    if (s == "true")  { k = TokKind::TRUE;   return true; }
    if (s == "false") { k = TokKind::FALSE;  return true; }
    if (s == "null")  { k = TokKind::NULL_T; return true; }
    if (s == "if")    { k = TokKind::IF;     return true; }
    if (s == "then")  { k = TokKind::THEN;   return true; }
    if (s == "else")  { k = TokKind::ELSE;   return true; }
    if (s == "end")   { k = TokKind::END;    return true; }
    if (s == "select"){ k = TokKind::SELECT; return true; }
    if (s == "and")   { k = TokKind::AND;    return true; }
    if (s == "or")    { k = TokKind::OR;     return true; }
    if (s == "not")   { k = TokKind::NOT;    return true; }
    return false;
}

std::vector<Tok> lex(const std::string& src) {
    std::vector<Tok> toks;
    Pos p{1, 1};
    size_t i_cur = 0;
    auto bump = [&](int n=1){
        for (int k=0; k<n; ++k) {
            if (i_cur >= src.size()) return;
            if (src[i_cur] == '\n') { p.line++; p.col = 1; }
            else { p.col++; }
            i_cur++;
        }
    };

    while (i_cur < src.size()) {
        char c = src[i_cur];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { bump(); continue; }
        Pos start = p;
        if (c == '.') {
            if (i_cur + 1 < src.size() && src[i_cur + 1] == '.') {
                Tok t{TokKind::RECURSE, "..", 0, start};
                toks.push_back(t);
                bump(2);
                continue;
            }
            Tok t{TokKind::DOT, ".", 0, start};
            toks.push_back(t);
            bump();
            continue;
        }
        if (c == '[') { Tok t{TokKind::LBRACKET, "[", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == ']') { Tok t{TokKind::RBRACKET, "]", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '{') { Tok t{TokKind::LBRACE, "{", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '}') { Tok t{TokKind::RBRACE, "}", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '(') { Tok t{TokKind::LPAREN, "(", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == ')') { Tok t{TokKind::RPAREN, ")", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '|') { Tok t{TokKind::PIPE, "|", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == ',') { Tok t{TokKind::COMMA, ",", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '?') { Tok t{TokKind::QUESTION, "?", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == ':') { Tok t{TokKind::COLON, ":", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '+') { Tok t{TokKind::PLUS, "+", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '-') { Tok t{TokKind::MINUS, "-", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '*') { Tok t{TokKind::STAR, "*", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '%') { Tok t{TokKind::PERCENT, "%", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '/') { Tok t{TokKind::SLASH, "/", 0, start}; toks.push_back(t); bump(); continue; }
        if (c == '=' && i_cur + 1 < src.size() && src[i_cur + 1] == '=') {
            Tok t{TokKind::EQ, "==", 0, start}; toks.push_back(t); bump(2); continue;
        }
        if (c == '!' && i_cur + 1 < src.size() && src[i_cur + 1] == '=') {
            Tok t{TokKind::NEQ, "!=", 0, start}; toks.push_back(t); bump(2); continue;
        }
        if (c == '<') {
            if (i_cur + 1 < src.size() && src[i_cur + 1] == '=') {
                Tok t{TokKind::LE, "<=", 0, start}; toks.push_back(t); bump(2); continue;
            }
            Tok t{TokKind::LT, "<", 0, start}; toks.push_back(t); bump(); continue;
        }
        if (c == '>') {
            if (i_cur + 1 < src.size() && src[i_cur + 1] == '=') {
                Tok t{TokKind::GE, ">=", 0, start}; toks.push_back(t); bump(2); continue;
            }
            Tok t{TokKind::GT, ">", 0, start}; toks.push_back(t); bump(); continue;
        }
        if (c == '"') {
            std::string s;
            bump();
            while (i_cur < src.size() && src[i_cur] != '"') {
                if (src[i_cur] == '\\' && i_cur + 1 < src.size()) {
                    char e = src[i_cur + 1];
                    switch (e) {
                        case 'n':  s += '\n'; break;
                        case 't':  s += '\t'; break;
                        case 'r':  s += '\r'; break;
                        case '"':  s += '"'; break;
                        case '\\': s += '\\'; break;
                        case '/':  s += '/'; break;
                        default:   s += e; break;
                    }
                    bump(2);
                } else {
                    s += src[i_cur];
                    bump();
                }
            }
            if (i_cur >= src.size()) throw CppJqError(start, "unterminated string");
            bump();
            Tok t{TokKind::STRING, s, 0, start};
            toks.push_back(t);
            continue;
        }
        if (std::isdigit((unsigned char)c)) {
            std::string ns;
            while (i_cur < src.size() && (std::isdigit((unsigned char)src[i_cur])
                   || src[i_cur] == '.' || src[i_cur] == 'e' || src[i_cur] == 'E')) {
                ns += src[i_cur];
                bump();
                if (i_cur < src.size() && (src[i_cur] == '+' || src[i_cur] == '-')
                    && !ns.empty() && (ns[ns.size() - 1] == 'e' || ns[ns.size() - 1] == 'E')) {
                    ns += src[i_cur];
                    bump();
                }
            }
            Tok t{TokKind::NUMBER, ns, 0, start};
            try { t.num = static_cast<int64_t>(std::stod(ns)); } catch (...) { t.num = 0; }
            toks.push_back(t);
            continue;
        }
        if (is_id_start(c)) {
            std::string id;
            while (i_cur < src.size() && is_id_cont(src[i_cur])) {
                id += src[i_cur];
                bump();
            }
            TokKind k = TokKind::IDENT;
            if (is_kw(id, k)) { Tok t{k, id, 0, start}; toks.push_back(t); }
            else              { Tok t{TokKind::IDENT, id, 0, start}; toks.push_back(t); }
            continue;
        }
        throw CppJqError(start, std::string("unexpected character: ") + c);
    }
    Tok t{TokKind::EOF_T, "", 0, p};
    toks.push_back(t);
    return toks;
}

}